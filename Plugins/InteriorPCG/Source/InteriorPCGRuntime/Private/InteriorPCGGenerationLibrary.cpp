// Copyright OutBreak. All Rights Reserved.

#include "InteriorPCGGenerationLibrary.h"

#include "InteriorPCGDataAssets.h"
#include "Engine/StaticMesh.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InteriorPCGGenerationLibrary)

namespace InteriorPCG::Private
{
	constexpr int32 EmptyCell = -1;
	constexpr int32 CorridorCell = -2;
	constexpr int32 CoreCell = -3;

	struct FRect2D
	{
		FVector2D Min = FVector2D::ZeroVector;
		FVector2D Max = FVector2D::ZeroVector;

		bool Intersects(const FRect2D& Other) const
		{
			return Min.X < Other.Max.X && Max.X > Other.Min.X && Min.Y < Other.Max.Y && Max.Y > Other.Min.Y;
		}

		bool Contains(const FRect2D& Other) const
		{
			return Other.Min.X >= Min.X && Other.Max.X <= Max.X && Other.Min.Y >= Min.Y && Other.Max.Y <= Max.Y;
		}
	};

	struct FResolvedCore
	{
		FName CoreID = NAME_None;
		EInteriorPCGModuleType ModuleType = EInteriorPCGModuleType::Stair;
		FIntPoint Min = FIntPoint::ZeroValue;
		FIntPoint Max = FIntPoint::ZeroValue;
	};

	struct FFloorState
	{
		int32 FloorIndex = 0;
		int32 FloorSeed = 0;
		int32 Columns = 0;
		int32 Rows = 0;
		bool bHorizontalCorridor = true;
		TArray<int32> Owners;
		TArray<FResolvedCore> Cores;
		TSet<uint64> DoorEdges;

		int32 Index(const int32 X, const int32 Y) const { return Y * Columns + X; }
		bool IsInside(const int32 X, const int32 Y) const { return X >= 0 && Y >= 0 && X < Columns && Y < Rows; }
		int32 Get(const int32 X, const int32 Y) const { return IsInside(X, Y) ? Owners[Index(X, Y)] : EmptyCell; }
		void Set(const int32 X, const int32 Y, const int32 Value)
		{
			if (IsInside(X, Y))
			{
				Owners[Index(X, Y)] = Value;
			}
		}
	};

	struct FPlacedProp
	{
		FInteriorPCGPlacement Placement;
		FRect2D Clearance;
	};

	uint32 MixSeed(const uint32 A, const uint32 B)
	{
		uint32 Value = A ^ (B + 0x9e3779b9u + (A << 6) + (A >> 2));
		Value ^= Value >> 16;
		Value *= 0x7feb352du;
		Value ^= Value >> 15;
		Value *= 0x846ca68bu;
		Value ^= Value >> 16;
		return Value;
	}

	int32 DeriveSeed(const int32 A, const int32 B, const int32 C = 0)
	{
		return static_cast<int32>(MixSeed(MixSeed(static_cast<uint32>(A), static_cast<uint32>(B)), static_cast<uint32>(C)) & 0x7fffffffu);
	}

	FString EnumName(const EInteriorPCGModuleType Value)
	{
		return StaticEnum<EInteriorPCGModuleType>()->GetNameStringByValue(static_cast<int64>(Value));
	}

	FString EnumName(const EInteriorPCGRoomType Value)
	{
		return StaticEnum<EInteriorPCGRoomType>()->GetNameStringByValue(static_cast<int64>(Value));
	}

	bool ContainsRoomType(const TArray<EInteriorPCGRoomType>& Types, const EInteriorPCGRoomType Type)
	{
		return Types.IsEmpty() || Types.Contains(Type);
	}

	uint64 MakeEdgeKey(const int32 X, const int32 Y, const int32 DX, const int32 DY)
	{
		const bool bVertical = DX != 0;
		const uint32 EdgeX = static_cast<uint32>(X + (DX > 0 ? 1 : 0));
		const uint32 EdgeY = static_cast<uint32>(Y + (DY > 0 ? 1 : 0));
		return (static_cast<uint64>(bVertical ? 1 : 0) << 63) |
			(static_cast<uint64>(EdgeX) << 31) | static_cast<uint64>(EdgeY);
	}

	FVector GridCellCenter(const int32 X, const int32 Y, const int32 FloorIndex, const int32 Columns, const int32 Rows,
		const double CellSize, const double FloorHeight)
	{
		const double OriginX = -static_cast<double>(Columns) * CellSize * 0.5;
		const double OriginY = -static_cast<double>(Rows) * CellSize * 0.5;
		return FVector(OriginX + (static_cast<double>(X) + 0.5) * CellSize,
			OriginY + (static_cast<double>(Y) + 0.5) * CellSize, static_cast<double>(FloorIndex) * FloorHeight);
	}

	FTransform ToWorld(const FTransform& LocalTransform, const FTransform& WorldTransform)
	{
		return LocalTransform * WorldTransform;
	}

	const FInteriorPCGAssetVariant* PickVariant(const TArray<FInteriorPCGAssetVariant>& Variants, FRandomStream& Stream)
	{
		double TotalWeight = 0.0;
		const FInteriorPCGAssetVariant* LastValidVariant = nullptr;
		for (const FInteriorPCGAssetVariant& Variant : Variants)
		{
			if (Variant.StaticMesh || Variant.ActorClass)
			{
				TotalWeight += FMath::Max(0.0f, Variant.Weight);
				LastValidVariant = &Variant;
			}
		}

		if (TotalWeight <= UE_DOUBLE_SMALL_NUMBER)
		{
			return nullptr;
		}

		double Choice = Stream.FRandRange(0.0, TotalWeight);
		for (const FInteriorPCGAssetVariant& Variant : Variants)
		{
			if (!Variant.StaticMesh && !Variant.ActorClass)
			{
				continue;
			}

			Choice -= FMath::Max(0.0f, Variant.Weight);
			if (Choice <= 0.0)
			{
				return &Variant;
			}
		}

		return LastValidVariant;
	}

	void ApplyVariant(FInteriorPCGPlacement& Placement, const FInteriorPCGAssetVariant* Variant,
		const FTransform& BaseLocalTransform, const FTransform& WorldTransform, FRandomStream& Stream)
	{
		FTransform LocalTransform = BaseLocalTransform;
		if (Variant)
		{
			Placement.VariantID = Variant->VariantID;
			Placement.StaticMesh = Variant->StaticMesh;
			Placement.AdditionalStaticMeshes = Variant->AdditionalStaticMeshes;
			Placement.ActorClass = Variant->ActorClass;
			Placement.bAllowInstancing = Variant->bAllowInstancing && !Variant->bInteractive;
			Placement.bInteractive = Variant->bInteractive;
			Placement.BoundsExtent = Variant->NominalSize.GetAbs() * 0.5;

			FTransform VariantOffset = Variant->PlacementOffset;
			if (!Variant->AllowedYawDegrees.IsEmpty())
			{
				const int32 YawIndex = Stream.RandRange(0, Variant->AllowedYawDegrees.Num() - 1);
				VariantOffset.ConcatenateRotation(FQuat(FRotator(0.0, Variant->AllowedYawDegrees[YawIndex], 0.0)));
			}
			LocalTransform = VariantOffset * BaseLocalTransform;
		}

		Placement.Transform = ToWorld(LocalTransform, WorldTransform);
	}

	void AddStructurePlacement(FInteriorPCGGenerationResult& OutResult, const UInteriorPCGGenerationProfile* Profile,
		const EInteriorPCGModuleType Type, const EInteriorPCGFloorBand FloorBand, const int32 FloorIndex,
		const int32 RoomID, const FTransform& LocalTransform,
		const FTransform& WorldTransform, const FVector& DefaultExtent, const int32 Seed)
	{
		FInteriorPCGPlacement Placement;
		Placement.Kind = EInteriorPCGPlacementKind::Structure;
		Placement.ModuleType = Type;
		Placement.FloorBand = FloorBand;
		Placement.FloorIndex = FloorIndex;
		Placement.RoomID = RoomID;
		Placement.BoundsExtent = DefaultExtent;
		Placement.Seed = Seed;

		FRandomStream Stream(Seed);
		const FInteriorPCGBuildingModuleDefinition* Module = Profile->BuildingModules ? Profile->BuildingModules->FindModule(Type, FloorBand) : nullptr;
		const FInteriorPCGAssetVariant* Variant = Module ? PickVariant(Module->Variants, Stream) : nullptr;
		ApplyVariant(Placement, Variant, LocalTransform, WorldTransform, Stream);
		OutResult.Placements.Add(MoveTemp(Placement));
	}

	EInteriorPCGFloorBand GetFloorBand(const int32 FloorIndex, const int32 NumFloors)
	{
		if (FloorIndex <= 0) return EInteriorPCGFloorBand::Ground;
		if (FloorIndex >= NumFloors - 1) return EInteriorPCGFloorBand::Top;
		return EInteriorPCGFloorBand::Middle;
	}

	void GetEntranceEdge(const FFloorState& State, const EInteriorPCGEntranceSide Side, const double NormalizedPosition,
		FIntPoint& OutCell, FIntPoint& OutDirection)
	{
		const double Position = FMath::Clamp(NormalizedPosition, 0.0, 1.0);
		switch (Side)
		{
		case EInteriorPCGEntranceSide::North:
			OutCell = FIntPoint(FMath::RoundToInt(Position * (State.Columns - 1)), State.Rows - 1);
			OutDirection = FIntPoint(0, 1);
			break;
		case EInteriorPCGEntranceSide::West:
			OutCell = FIntPoint(0, FMath::RoundToInt(Position * (State.Rows - 1)));
			OutDirection = FIntPoint(-1, 0);
			break;
		case EInteriorPCGEntranceSide::East:
			OutCell = FIntPoint(State.Columns - 1, FMath::RoundToInt(Position * (State.Rows - 1)));
			OutDirection = FIntPoint(1, 0);
			break;
		case EInteriorPCGEntranceSide::South:
		default:
			OutCell = FIntPoint(FMath::RoundToInt(Position * (State.Columns - 1)), 0);
			OutDirection = FIntPoint(0, -1);
			break;
		}
	}

	void GetMainEntranceEdge(const FFloorState& State, const UInteriorPCGBuildingRuleSet& Rules,
		FIntPoint& OutCell, FIntPoint& OutDirection)
	{
		GetEntranceEdge(State, Rules.MainEntranceSide, Rules.MainEntrancePosition, OutCell, OutDirection);
	}

	TArray<FResolvedCore> ResolveCores(const UInteriorPCGBuildingRuleSet& Rules, const int32 Columns, const int32 Rows)
	{
		TArray<FResolvedCore> Result;
		for (const FInteriorPCGVerticalCoreDefinition& Core : Rules.VerticalCores)
		{
			FResolvedCore& Resolved = Result.Emplace_GetRef();
			Resolved.CoreID = Core.CoreID;
			Resolved.ModuleType = Core.ModuleType;
			const int32 Width = FMath::Clamp(Core.SizeInCells.X, 1, FMath::Max(1, Columns - 2));
			const int32 Height = FMath::Clamp(Core.SizeInCells.Y, 1, FMath::Max(1, Rows - 2));
			const int32 CenterX = FMath::RoundToInt(FMath::Clamp(Core.NormalizedPosition.X, 0.0, 1.0) * (Columns - 1));
			const int32 CenterY = FMath::RoundToInt(FMath::Clamp(Core.NormalizedPosition.Y, 0.0, 1.0) * (Rows - 1));
			Resolved.Min.X = FMath::Clamp(CenterX - Width / 2, 1, FMath::Max(1, Columns - Width - 1));
			Resolved.Min.Y = FMath::Clamp(CenterY - Height / 2, 1, FMath::Max(1, Rows - Height - 1));
			Resolved.Max = Resolved.Min + FIntPoint(Width, Height);
		}
		return Result;
	}

	void MarkCoreAndBypass(FFloorState& State)
	{
		for (const FResolvedCore& Core : State.Cores)
		{
			for (int32 Y = Core.Min.Y; Y < Core.Max.Y; ++Y)
			{
				for (int32 X = Core.Min.X; X < Core.Max.X; ++X)
				{
					State.Set(X, Y, CoreCell);
				}
			}
		}

		// A one-cell ring keeps a corridor stripe connected when a vertical core cuts through it.
		// Compact 2xN/Nx2 envelopes cannot afford the ring; the main corridor already reaches both sides.
		if (State.Columns < 4 || State.Rows < 4)
		{
			return;
		}
		for (const FResolvedCore& Core : State.Cores)
		{
			for (int32 X = Core.Min.X - 1; X <= Core.Max.X; ++X)
			{
				if (State.Get(X, Core.Min.Y - 1) == EmptyCell) State.Set(X, Core.Min.Y - 1, CorridorCell);
				if (State.Get(X, Core.Max.Y) == EmptyCell) State.Set(X, Core.Max.Y, CorridorCell);
			}
			for (int32 Y = Core.Min.Y; Y < Core.Max.Y; ++Y)
			{
				if (State.Get(Core.Min.X - 1, Y) == EmptyCell) State.Set(Core.Min.X - 1, Y, CorridorCell);
				if (State.Get(Core.Max.X, Y) == EmptyCell) State.Set(Core.Max.X, Y, CorridorCell);
			}
		}
	}

	void MarkCorridor(FFloorState& State, const UInteriorPCGBuildingRuleSet& Rules)
	{
		const int32 Width = FMath::Clamp(Rules.CorridorWidthInCells, 1,
			FMath::Max(1, (State.bHorizontalCorridor ? State.Rows : State.Columns) - 2));
		if (State.bHorizontalCorridor)
		{
			const int32 MinY = (State.Rows - Width) / 2;
			for (int32 Y = MinY; Y < MinY + Width; ++Y)
			{
				for (int32 X = 0; X < State.Columns; ++X)
				{
					if (State.Get(X, Y) == EmptyCell) State.Set(X, Y, CorridorCell);
				}
			}

		}
		else
		{
			const int32 MinX = (State.Columns - Width) / 2;
			for (int32 X = MinX; X < MinX + Width; ++X)
			{
				for (int32 Y = 0; Y < State.Rows; ++Y)
				{
					if (State.Get(X, Y) == EmptyCell) State.Set(X, Y, CorridorCell);
				}
			}
		}

		if (State.FloorIndex == 0)
		{
			auto MarkEntranceConnector = [&State](FIntPoint Cell)
			{
				const FIntPoint Target(State.Columns / 2, State.Rows / 2);
				for (int32 Guard = 0; Guard < State.Columns + State.Rows + 2; ++Guard)
				{
					if (State.Get(Cell.X, Cell.Y) == EmptyCell) State.Set(Cell.X, Cell.Y, CorridorCell);
					if (State.Get(Cell.X, Cell.Y) == CorridorCell && Cell == Target) break;
					if (Cell.X != Target.X) Cell.X += Cell.X < Target.X ? 1 : -1;
					else if (Cell.Y != Target.Y) Cell.Y += Cell.Y < Target.Y ? 1 : -1;
					else break;
				}
			};

			FIntPoint Cell;
			FIntPoint ExteriorDirection;
			GetMainEntranceEdge(State, Rules, Cell, ExteriorDirection);
			MarkEntranceConnector(Cell);
			for (const FInteriorPCGExteriorEntranceDefinition& Entrance : Rules.AdditionalEntrances)
			{
				GetEntranceEdge(State, Entrance.Side, Entrance.NormalizedPosition, Cell, ExteriorDirection);
				MarkEntranceConnector(Cell);
			}
		}

		MarkCoreAndBypass(State);
	}

	void AddConnectedRoomsInRect(FFloorState& State, FInteriorPCGGenerationResult& OutResult,
		const FIntPoint RectMin, const FIntPoint RectMax, int32& NextLocalRoomID, const UInteriorPCGBuildingRuleSet& Rules,
		const FInteriorPCGGenerationOptions& Options)
	{
		const FIntPoint Directions[] = {FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1)};
		for (int32 StartY = RectMin.Y; StartY < RectMax.Y; ++StartY)
		{
			for (int32 StartX = RectMin.X; StartX < RectMax.X; ++StartX)
			{
				if (State.Get(StartX, StartY) != EmptyCell)
				{
					continue;
				}

				const int32 RoomID = (State.FloorIndex + 1) * 10000 + (++NextLocalRoomID);
				TArray<FIntPoint> Queue;
				Queue.Add(FIntPoint(StartX, StartY));
				State.Set(StartX, StartY, RoomID);
				FIntPoint CellMin(StartX, StartY);
				FIntPoint CellMax(StartX + 1, StartY + 1);
				int32 CellCount = 0;
				bool bExterior = false;

				for (int32 QueueIndex = 0; QueueIndex < Queue.Num(); ++QueueIndex)
				{
					const FIntPoint Cell = Queue[QueueIndex];
					++CellCount;
					CellMin.X = FMath::Min(CellMin.X, Cell.X);
					CellMin.Y = FMath::Min(CellMin.Y, Cell.Y);
					CellMax.X = FMath::Max(CellMax.X, Cell.X + 1);
					CellMax.Y = FMath::Max(CellMax.Y, Cell.Y + 1);
					bExterior |= Cell.X == 0 || Cell.Y == 0 || Cell.X == State.Columns - 1 || Cell.Y == State.Rows - 1;

					for (const FIntPoint Direction : Directions)
					{
						const FIntPoint Next = Cell + Direction;
						if (Next.X >= RectMin.X && Next.Y >= RectMin.Y && Next.X < RectMax.X && Next.Y < RectMax.Y &&
							State.Get(Next.X, Next.Y) == EmptyCell)
						{
							State.Set(Next.X, Next.Y, RoomID);
							Queue.Add(Next);
						}
					}
				}

				FInteriorPCGRoom& Room = OutResult.Rooms.Emplace_GetRef();
				Room.RoomID = RoomID;
				Room.FloorIndex = State.FloorIndex;
				Room.GridMin = CellMin;
				Room.GridMax = CellMax;
				Room.Area = static_cast<double>(CellCount) * Rules.CellSize * Rules.CellSize;
				Room.bTouchesExterior = bExterior;
				const FVector MinCenter = GridCellCenter(CellMin.X, CellMin.Y, State.FloorIndex, State.Columns, State.Rows,
					Rules.CellSize, Rules.FloorHeight);
				const FVector MaxCenter = GridCellCenter(CellMax.X - 1, CellMax.Y - 1, State.FloorIndex, State.Columns, State.Rows,
					Rules.CellSize, Rules.FloorHeight);
				const FVector LocalCenter = (MinCenter + MaxCenter) * 0.5;
				Room.Center = Options.WorldTransform.TransformPosition(LocalCenter);
				Room.Extents = FVector((CellMax.X - CellMin.X) * Rules.CellSize * 0.5,
					(CellMax.Y - CellMin.Y) * Rules.CellSize * 0.5, Rules.FloorHeight * 0.5);
				Room.Seeds = UInteriorPCGGenerationLibrary::MakeSeedBundle(Options.BuildingSeed, State.FloorIndex, RoomID);
			}
		}
	}

	void PartitionRooms(FFloorState& State, FInteriorPCGGenerationResult& OutResult,
		const UInteriorPCGBuildingRuleSet& Rules, const FInteriorPCGGenerationOptions& Options)
	{
		FRandomStream Stream(State.FloorSeed);
		const int32 MinLength = FMath::Max(1, Rules.MinimumRoomLengthInCells);
		const int32 MaxLength = FMath::Max(MinLength, Rules.MaximumRoomLengthInCells);
		int32 NextLocalRoomID = 0;

		if (State.bHorizontalCorridor)
		{
			int32 CorridorMin = State.Rows;
			int32 CorridorMax = 0;
			for (int32 Y = 0; Y < State.Rows; ++Y)
			{
				if (State.Get(0, Y) == CorridorCell)
				{
					CorridorMin = FMath::Min(CorridorMin, Y);
					CorridorMax = FMath::Max(CorridorMax, Y + 1);
				}
			}
			CorridorMin = FMath::Clamp(CorridorMin, 1, State.Rows - 1);
			CorridorMax = FMath::Clamp(CorridorMax, CorridorMin, State.Rows - 1);

			for (int32 X = 0; X < State.Columns;)
			{
				const int32 Length = FMath::Min(Stream.RandRange(MinLength, MaxLength), State.Columns - X);
				AddConnectedRoomsInRect(State, OutResult, FIntPoint(X, 0), FIntPoint(X + Length, CorridorMin),
					NextLocalRoomID, Rules, Options);
				AddConnectedRoomsInRect(State, OutResult, FIntPoint(X, CorridorMax), FIntPoint(X + Length, State.Rows),
					NextLocalRoomID, Rules, Options);
				X += Length;
			}
		}
		else
		{
			int32 CorridorMin = State.Columns;
			int32 CorridorMax = 0;
			for (int32 X = 0; X < State.Columns; ++X)
			{
				if (State.Get(X, 0) == CorridorCell)
				{
					CorridorMin = FMath::Min(CorridorMin, X);
					CorridorMax = FMath::Max(CorridorMax, X + 1);
				}
			}
			CorridorMin = FMath::Clamp(CorridorMin, 1, State.Columns - 1);
			CorridorMax = FMath::Clamp(CorridorMax, CorridorMin, State.Columns - 1);

			for (int32 Y = 0; Y < State.Rows;)
			{
				const int32 Length = FMath::Min(Stream.RandRange(MinLength, MaxLength), State.Rows - Y);
				AddConnectedRoomsInRect(State, OutResult, FIntPoint(0, Y), FIntPoint(CorridorMin, Y + Length),
					NextLocalRoomID, Rules, Options);
				AddConnectedRoomsInRect(State, OutResult, FIntPoint(CorridorMax, Y), FIntPoint(State.Columns, Y + Length),
					NextLocalRoomID, Rules, Options);
				Y += Length;
			}
		}

		// Defensive fill for unusual core/corridor arrangements.
		AddConnectedRoomsInRect(State, OutResult, FIntPoint(0, 0), FIntPoint(State.Columns, State.Rows),
			NextLocalRoomID, Rules, Options);
	}

	EInteriorPCGRoomType PickRoomType(const FInteriorPCGRoom& Room,
		const TArray<FInteriorPCGWeightedRoomType>& Types, FRandomStream& Stream)
	{
		double Total = 0.0;
		for (const FInteriorPCGWeightedRoomType& Entry : Types)
		{
			const bool bAreaOK = Room.Area >= static_cast<double>(Entry.MinimumAreaSquareMeters) * 10000.0;
			if (bAreaOK && (!Entry.bRequiresExteriorWall || Room.bTouchesExterior))
			{
				Total += FMath::Max(0.0f, Entry.Weight);
			}
		}
		if (Total <= UE_DOUBLE_SMALL_NUMBER)
		{
			return Room.bTouchesExterior ? EInteriorPCGRoomType::Office : EInteriorPCGRoomType::Utility;
		}

		double Choice = Stream.FRandRange(0.0, Total);
		for (const FInteriorPCGWeightedRoomType& Entry : Types)
		{
			const bool bAreaOK = Room.Area >= static_cast<double>(Entry.MinimumAreaSquareMeters) * 10000.0;
			if (!bAreaOK || (Entry.bRequiresExteriorWall && !Room.bTouchesExterior)) continue;
			Choice -= FMath::Max(0.0f, Entry.Weight);
			if (Choice <= 0.0) return Entry.RoomType;
		}
		return Types.Last().RoomType;
	}

	void ClassifyRooms(FFloorState& State, FInteriorPCGGenerationResult& OutResult,
		const UInteriorPCGBuildingRuleSet& Rules, const FInteriorPCGGenerationOptions& Options, const int32 FirstRoomIndex)
	{
		const TArray<FInteriorPCGWeightedRoomType>& Types = State.FloorIndex == 0 ? Rules.FirstFloorRoomTypes : Rules.RepeatFloorRoomTypes;
		int32 LobbyIndex = INDEX_NONE;
		double LobbyDistance = TNumericLimits<double>::Max();
		FIntPoint EntranceCell;
		FIntPoint EntranceDirection;
		GetMainEntranceEdge(State, Rules, EntranceCell, EntranceDirection);
		const FVector EntranceLocal = GridCellCenter(EntranceCell.X, EntranceCell.Y, 0, State.Columns, State.Rows, Rules.CellSize, Rules.FloorHeight);

		for (int32 Index = FirstRoomIndex; Index < OutResult.Rooms.Num(); ++Index)
		{
			FInteriorPCGRoom& Room = OutResult.Rooms[Index];
			FRandomStream Stream(Room.Seeds.RoomSeed);
			Room.RoomType = Types.IsEmpty() ? (Room.bTouchesExterior ? EInteriorPCGRoomType::Office : EInteriorPCGRoomType::Utility)
				: PickRoomType(Room, Types, Stream);
			if (State.FloorIndex == 0)
			{
				const FVector LocalCenter = Options.WorldTransform.InverseTransformPosition(Room.Center);
				const double Distance = FVector::DistSquared2D(LocalCenter, EntranceLocal);
				if (Distance < LobbyDistance)
				{
					LobbyDistance = Distance;
					LobbyIndex = Index;
				}
			}
		}

		if (LobbyIndex != INDEX_NONE)
		{
			OutResult.Rooms[LobbyIndex].RoomType = EInteriorPCGRoomType::Lobby;
		}
	}

	FTransform MakeOpeningLocalTransform(const int32 X, const int32 Y, const int32 DX, const int32 DY,
		const FFloorState& State, const UInteriorPCGBuildingRuleSet& Rules)
	{
		FVector Location = GridCellCenter(X, Y, State.FloorIndex, State.Columns, State.Rows, Rules.CellSize, Rules.FloorHeight);
		Location += FVector(DX, DY, 0.0) * (Rules.CellSize * 0.5);
		const double Yaw = DX != 0 ? 90.0 : 0.0;
		return FTransform(FRotator(0.0, Yaw, 0.0), Location);
	}

	void AddPortals(FFloorState& State, FInteriorPCGGenerationResult& OutResult,
		const UInteriorPCGBuildingRuleSet& Rules, const FInteriorPCGGenerationOptions& Options, const int32 FirstRoomIndex)
	{
		const FIntPoint Directions[] = {FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1)};
		for (int32 RoomIndex = FirstRoomIndex; RoomIndex < OutResult.Rooms.Num(); ++RoomIndex)
		{
			FInteriorPCGRoom& Room = OutResult.Rooms[RoomIndex];
			TArray<TPair<FIntPoint, FIntPoint>> Candidates;
			for (int32 Y = 0; Y < State.Rows; ++Y)
			{
				for (int32 X = 0; X < State.Columns; ++X)
				{
					if (State.Get(X, Y) != Room.RoomID) continue;
					for (const FIntPoint Direction : Directions)
					{
						if (State.Get(X + Direction.X, Y + Direction.Y) == CorridorCell)
						{
							Candidates.Emplace(FIntPoint(X, Y), Direction);
						}
					}
				}
			}

			if (Candidates.IsEmpty())
			{
				OutResult.Warnings.Add(FString::Printf(TEXT("Room %d (%s) has no direct corridor connection."),
					Room.RoomID, *EnumName(Room.RoomType)));
				continue;
			}

			const FVector2D RoomGridCenter((Room.GridMin.X + Room.GridMax.X - 1) * 0.5, (Room.GridMin.Y + Room.GridMax.Y - 1) * 0.5);
			Candidates.Sort([RoomGridCenter](const TPair<FIntPoint, FIntPoint>& A, const TPair<FIntPoint, FIntPoint>& B)
			{
				return FVector2D::DistSquared(FVector2D(A.Key.X, A.Key.Y), RoomGridCenter) <
					FVector2D::DistSquared(FVector2D(B.Key.X, B.Key.Y), RoomGridCenter);
			});
			const TPair<FIntPoint, FIntPoint>& Chosen = Candidates[Candidates.Num() / 2];
			const FTransform LocalTransform = MakeOpeningLocalTransform(Chosen.Key.X, Chosen.Key.Y, Chosen.Value.X, Chosen.Value.Y, State, Rules);

			FInteriorPCGPortal& Portal = OutResult.Portals.Emplace_GetRef();
			Portal.ModuleType = EInteriorPCGModuleType::Door;
			Portal.FloorIndex = State.FloorIndex;
			Portal.RoomID = Room.RoomID;
			Portal.Transform = ToWorld(LocalTransform, Options.WorldTransform);
			Portal.InwardDirection = Options.WorldTransform.TransformVectorNoScale(FVector(-Chosen.Value.X, -Chosen.Value.Y, 0.0)).GetSafeNormal();
			Room.bConnectedToCorridor = true;
			State.DoorEdges.Add(MakeEdgeKey(Chosen.Key.X, Chosen.Key.Y, Chosen.Value.X, Chosen.Value.Y));
		}

		if (State.FloorIndex == 0)
		{
			auto AddExteriorEntrance = [&State, &Rules, &Options, &OutResult](const EInteriorPCGEntranceSide Side, const double Position)
			{
				FIntPoint EntranceCell;
				FIntPoint EntranceDirection;
				GetEntranceEdge(State, Side, Position, EntranceCell, EntranceDirection);
				const uint64 EdgeKey = MakeEdgeKey(EntranceCell.X, EntranceCell.Y, EntranceDirection.X, EntranceDirection.Y);
				if (State.DoorEdges.Contains(EdgeKey)) return;
				const FTransform LocalTransform = MakeOpeningLocalTransform(EntranceCell.X, EntranceCell.Y,
					EntranceDirection.X, EntranceDirection.Y, State, Rules);
				FInteriorPCGPortal& Portal = OutResult.Portals.Emplace_GetRef();
				Portal.ModuleType = EInteriorPCGModuleType::Door;
				Portal.FloorIndex = 0;
				Portal.RoomID = INDEX_NONE;
				Portal.Transform = ToWorld(LocalTransform, Options.WorldTransform);
				Portal.InwardDirection = Options.WorldTransform.TransformVectorNoScale(
					FVector(-EntranceDirection.X, -EntranceDirection.Y, 0.0)).GetSafeNormal();
				State.DoorEdges.Add(EdgeKey);
			};
			AddExteriorEntrance(Rules.MainEntranceSide, Rules.MainEntrancePosition);
			for (const FInteriorPCGExteriorEntranceDefinition& Entrance : Rules.AdditionalEntrances)
			{
				AddExteriorEntrance(Entrance.Side, Entrance.NormalizedPosition);
			}
		}
	}

	void EmitStructure(FFloorState& State, FInteriorPCGGenerationResult& OutResult,
		const UInteriorPCGGenerationProfile* Profile, const FInteriorPCGGenerationOptions& Options)
	{
		const UInteriorPCGBuildingRuleSet& Rules = *Profile->BuildingRules;
		const double CellSize = Rules.CellSize;
		const EInteriorPCGFloorBand FloorBand = GetFloorBand(State.FloorIndex, Options.NumFloors);
		const FVector TileExtent(CellSize * 0.5, CellSize * 0.5, 10.0);
		for (int32 Y = 0; Y < State.Rows; ++Y)
		{
			for (int32 X = 0; X < State.Columns; ++X)
			{
				const FVector Base = GridCellCenter(X, Y, State.FloorIndex, State.Columns, State.Rows, CellSize, Rules.FloorHeight);
				const int32 Owner = State.Get(X, Y);
				const EInteriorPCGModuleType FloorType = Rules.bUseFloorOpeningsAtVerticalCores && Owner == CoreCell && State.FloorIndex > 0
					? EInteriorPCGModuleType::FloorOpening : EInteriorPCGModuleType::Floor;
				AddStructurePlacement(OutResult, Profile, FloorType, FloorBand, State.FloorIndex, Owner,
					FTransform(FRotator::ZeroRotator, Base), Options.WorldTransform, TileExtent,
					DeriveSeed(State.FloorSeed, State.Index(X, Y), 11));
				if (Rules.bGenerateCeilingTiles)
				{
					const FVector CeilingLocation = Base + FVector(0.0, 0.0, Rules.FloorHeight);
					AddStructurePlacement(OutResult, Profile, EInteriorPCGModuleType::Ceiling, FloorBand, State.FloorIndex, Owner,
						FTransform(FRotator::ZeroRotator, CeilingLocation), Options.WorldTransform, TileExtent,
						DeriveSeed(State.FloorSeed, State.Index(X, Y), 13));
				}
				if (Rules.bGenerateRoofTiles && State.FloorIndex == Options.NumFloors - 1)
				{
					const FVector RoofLocation = Base + FVector(0.0, 0.0, Rules.FloorHeight);
					AddStructurePlacement(OutResult, Profile, EInteriorPCGModuleType::Roof, EInteriorPCGFloorBand::Roof,
						State.FloorIndex, Owner, FTransform(FRotator::ZeroRotator, RoofLocation), Options.WorldTransform,
						TileExtent, DeriveSeed(State.FloorSeed, State.Index(X, Y), 15));
				}
			}
		}

		if (Rules.RoofDecorationCount > 0 && State.FloorIndex == Options.NumFloors - 1)
		{
			FRandomStream DecorationStream(DeriveSeed(State.FloorSeed, Rules.RoofDecorationCount, 16));
			TSet<int32> UsedCells;
			for (int32 DecorationIndex = 0;
				DecorationIndex < FMath::Min(Rules.RoofDecorationCount, State.Columns * State.Rows); ++DecorationIndex)
			{
				int32 CellIndex = DecorationStream.RandRange(0, State.Columns * State.Rows - 1);
				while (UsedCells.Contains(CellIndex)) CellIndex = (CellIndex + 1) % (State.Columns * State.Rows);
				UsedCells.Add(CellIndex);
				const int32 X = CellIndex % State.Columns;
				const int32 Y = CellIndex / State.Columns;
				const FVector Location = GridCellCenter(X, Y, State.FloorIndex, State.Columns, State.Rows, CellSize, Rules.FloorHeight) +
					FVector(0.0, 0.0, Rules.FloorHeight);
				AddStructurePlacement(OutResult, Profile, EInteriorPCGModuleType::RoofDecoration, EInteriorPCGFloorBand::Roof,
					State.FloorIndex, INDEX_NONE, FTransform(FRotator::ZeroRotator, Location), Options.WorldTransform,
					FVector(CellSize * 0.25, CellSize * 0.25, Rules.FloorHeight * 0.25),
					DeriveSeed(State.FloorSeed, DecorationIndex, 16));
			}
		}

		for (const FResolvedCore& Core : State.Cores)
		{
			const FVector A = GridCellCenter(Core.Min.X, Core.Min.Y, State.FloorIndex, State.Columns, State.Rows, CellSize, Rules.FloorHeight);
			const FVector B = GridCellCenter(Core.Max.X - 1, Core.Max.Y - 1, State.FloorIndex, State.Columns, State.Rows, CellSize, Rules.FloorHeight);
			const FVector Location = (A + B) * 0.5;
			const FVector Extent((Core.Max.X - Core.Min.X) * CellSize * 0.5, (Core.Max.Y - Core.Min.Y) * CellSize * 0.5,
				Rules.FloorHeight * 0.5);
			if (Core.ModuleType != EInteriorPCGModuleType::Stair || State.FloorIndex < Options.NumFloors - 1)
			{
				AddStructurePlacement(OutResult, Profile, Core.ModuleType, FloorBand, State.FloorIndex, INDEX_NONE,
					FTransform(FRotator::ZeroRotator, Location), Options.WorldTransform, Extent,
					DeriveSeed(State.FloorSeed, GetTypeHash(Core.CoreID), 17));
			}
		}

		for (const FInteriorPCGPortal& Portal : OutResult.Portals)
		{
			if (Portal.FloorIndex != State.FloorIndex || Portal.ModuleType != EInteriorPCGModuleType::Door) continue;
			const FTransform LocalTransform = Portal.Transform.GetRelativeTransform(Options.WorldTransform);
			AddStructurePlacement(OutResult, Profile, EInteriorPCGModuleType::Door, FloorBand, State.FloorIndex, Portal.RoomID,
				LocalTransform, Options.WorldTransform, FVector(CellSize * 0.5, 20.0, Rules.FloorHeight * 0.5),
				DeriveSeed(State.FloorSeed, Portal.RoomID, 19));
		}

		if (Rules.bUseDedicatedExteriorCorners)
		{
			const double HalfWidth = State.Columns * CellSize * 0.5;
			const double HalfLength = State.Rows * CellSize * 0.5;
			const double Z = State.FloorIndex * Rules.FloorHeight;
			const TPair<FVector, double> Corners[] = {
				{FVector(-HalfWidth, HalfLength, Z), 0.0},
				{FVector(HalfWidth, HalfLength, Z), -90.0},
				{FVector(-HalfWidth, -HalfLength, Z), 90.0},
				{FVector(HalfWidth, -HalfLength, Z), 180.0}
			};
			for (int32 CornerIndex = 0; CornerIndex < UE_ARRAY_COUNT(Corners); ++CornerIndex)
			{
				AddStructurePlacement(OutResult, Profile, EInteriorPCGModuleType::ExteriorCorner, FloorBand,
					State.FloorIndex, INDEX_NONE, FTransform(FRotator(0.0, Corners[CornerIndex].Value, 0.0), Corners[CornerIndex].Key),
					Options.WorldTransform, FVector(CellSize * 0.5, CellSize * 0.5, Rules.FloorHeight * 0.5),
					DeriveSeed(State.FloorSeed, CornerIndex, 21));
			}
		}

		TSet<uint64> EmittedEdges;
		const FIntPoint Directions[] = {FIntPoint(-1, 0), FIntPoint(1, 0), FIntPoint(0, -1), FIntPoint(0, 1)};
		for (int32 Y = 0; Y < State.Rows; ++Y)
		{
			for (int32 X = 0; X < State.Columns; ++X)
			{
				const int32 Owner = State.Get(X, Y);
				for (const FIntPoint Direction : Directions)
				{
					const uint64 EdgeKey = MakeEdgeKey(X, Y, Direction.X, Direction.Y);
					if (EmittedEdges.Contains(EdgeKey)) continue;
					EmittedEdges.Add(EdgeKey);

					const bool bOutside = !State.IsInside(X + Direction.X, Y + Direction.Y);
					const int32 OtherOwner = State.Get(X + Direction.X, Y + Direction.Y);
					if (!bOutside && OtherOwner == Owner) continue;
					if (!bOutside && Owner == CoreCell && OtherOwner == CoreCell) continue;
					if (State.DoorEdges.Contains(EdgeKey)) continue;
					if (bOutside && Rules.bUseDedicatedExteriorCorners && Rules.ExteriorCornerSpanInCells > 0)
					{
						const int32 Coordinate = Direction.X != 0 ? Y : X;
						const int32 SideLength = Direction.X != 0 ? State.Rows : State.Columns;
						if (Coordinate < Rules.ExteriorCornerSpanInCells || Coordinate >= SideLength - Rules.ExteriorCornerSpanInCells)
						{
							continue;
						}
					}

					const FTransform LocalTransform = MakeOpeningLocalTransform(X, Y, Direction.X, Direction.Y, State, Rules);
					EInteriorPCGModuleType ModuleType = bOutside ? EInteriorPCGModuleType::ExteriorWall : EInteriorPCGModuleType::InteriorWall;

					if (bOutside && Owner >= 0 && Rules.MinimumWindowSpacingInCells > 0)
					{
						const int32 EdgeCoordinate = Direction.X != 0 ? Y : X;
						FRandomStream WindowStream(DeriveSeed(State.FloorSeed, static_cast<int32>(EdgeKey), 23));
						if ((EdgeCoordinate % Rules.MinimumWindowSpacingInCells) == 0 && WindowStream.FRand() <= Rules.ExteriorWindowChance)
						{
							ModuleType = EInteriorPCGModuleType::Window;
							FInteriorPCGPortal& Portal = OutResult.Portals.Emplace_GetRef();
							Portal.ModuleType = EInteriorPCGModuleType::Window;
							Portal.FloorIndex = State.FloorIndex;
							Portal.RoomID = Owner;
							Portal.Transform = ToWorld(LocalTransform, Options.WorldTransform);
							Portal.InwardDirection = Options.WorldTransform.TransformVectorNoScale(FVector(-Direction.X, -Direction.Y, 0.0)).GetSafeNormal();
						}
					}

					AddStructurePlacement(OutResult, Profile, ModuleType, FloorBand, State.FloorIndex, Owner, LocalTransform,
						Options.WorldTransform, FVector(CellSize * 0.5, 20.0, Rules.FloorHeight * 0.5),
						DeriveSeed(State.FloorSeed, static_cast<int32>(EdgeKey), 29));
				}
			}
		}
	}

	const FInteriorPCGFunctionalSetDefinition* PickFunctionalSet(const FInteriorPCGRoom& Room,
		const UInteriorPCGInteriorRuleSet& Rules, FRandomStream& Stream)
	{
		double Total = 0.0;
		for (const FInteriorPCGFunctionalSetDefinition& Set : Rules.FunctionalSets)
		{
			const double AreaSquareMeters = Room.Area / 10000.0;
			if (ContainsRoomType(Set.AllowedRoomTypes, Room.RoomType) && AreaSquareMeters >= Set.MinimumAreaSquareMeters &&
				(Set.MaximumAreaSquareMeters <= 0.0f || AreaSquareMeters <= Set.MaximumAreaSquareMeters))
			{
				Total += FMath::Max(0.0f, Set.Weight);
			}
		}
		if (Total <= UE_DOUBLE_SMALL_NUMBER) return nullptr;

		double Choice = Stream.FRandRange(0.0, Total);
		for (const FInteriorPCGFunctionalSetDefinition& Set : Rules.FunctionalSets)
		{
			const double AreaSquareMeters = Room.Area / 10000.0;
			if (!ContainsRoomType(Set.AllowedRoomTypes, Room.RoomType) || AreaSquareMeters < Set.MinimumAreaSquareMeters ||
				(Set.MaximumAreaSquareMeters > 0.0f && AreaSquareMeters > Set.MaximumAreaSquareMeters)) continue;
			Choice -= FMath::Max(0.0f, Set.Weight);
			if (Choice <= 0.0) return &Set;
		}
		return nullptr;
	}

	FRect2D MakeClearanceRect(const FVector& Location, const FVector2D& Footprint, const float SideClearance,
		const float FrontClearance, const double YawDegrees)
	{
		const double Radians = FMath::DegreesToRadians(YawDegrees);
		const double AbsCos = FMath::Abs(FMath::Cos(Radians));
		const double AbsSin = FMath::Abs(FMath::Sin(Radians));
		const FVector2D LocalHalf(Footprint.X * 0.5 + FrontClearance * 0.5, Footprint.Y * 0.5 + SideClearance);
		const FVector2D WorldHalf(AbsCos * LocalHalf.X + AbsSin * LocalHalf.Y, AbsSin * LocalHalf.X + AbsCos * LocalHalf.Y);
		const FVector2D Forward(FMath::Cos(Radians), FMath::Sin(Radians));
		const FVector2D Center(Location.X, Location.Y);
		const FVector2D ShiftedCenter = Center + Forward * (FrontClearance * 0.5);
		return {ShiftedCenter - WorldHalf, ShiftedCenter + WorldHalf};
	}

	FRect2D MakeSegmentRect(const FVector2D A, const FVector2D B, const double HalfWidth)
	{
		return {FVector2D(FMath::Min(A.X, B.X) - HalfWidth, FMath::Min(A.Y, B.Y) - HalfWidth),
			FVector2D(FMath::Max(A.X, B.X) + HalfWidth, FMath::Max(A.Y, B.Y) + HalfWidth)};
	}

	const FInteriorPCGPortal* FindPortal(const FInteriorPCGGenerationResult& Result, const int32 RoomID,
		const EInteriorPCGModuleType Type)
	{
		return Result.Portals.FindByPredicate([RoomID, Type](const FInteriorPCGPortal& Portal)
		{
			return Portal.RoomID == RoomID && Portal.ModuleType == Type;
		});
	}

	bool IsPlacementClear(const FRect2D& Candidate, const FRect2D& RoomBounds, const TArray<FRect2D>& Reserved,
		const TArray<FPlacedProp>& Placed, const bool bIgnorePath)
	{
		if (!RoomBounds.Contains(Candidate)) return false;
		if (!bIgnorePath)
		{
			for (const FRect2D& Zone : Reserved)
			{
				if (Candidate.Intersects(Zone)) return false;
			}
		}
		for (const FPlacedProp& Existing : Placed)
		{
			if (Candidate.Intersects(Existing.Clearance)) return false;
		}
		return true;
	}

	bool MeetsPortalDistances(const FVector& LocalLocation, const FInteriorPCGPropDefinition& Prop,
		const FInteriorPCGRoom& Room, const FInteriorPCGGenerationOptions& Options,
		const FInteriorPCGGenerationResult& Result)
	{
		for (const FInteriorPCGPortal& Portal : Result.Portals)
		{
			if (Portal.RoomID != Room.RoomID) continue;
			const FVector PortalLocal = Options.WorldTransform.InverseTransformPosition(Portal.Transform.GetLocation());
			const double Distance = FVector::Dist2D(LocalLocation, PortalLocal);
			if (Portal.ModuleType == EInteriorPCGModuleType::Door && Distance < Prop.MinimumDoorDistance) return false;
			if (Portal.ModuleType == EInteriorPCGModuleType::Window && Distance < Prop.MinimumWindowDistance) return false;
		}
		return true;
	}

	bool ResolveAnchorTransform(const EInteriorPCGAnchorType Anchor, const FInteriorPCGPropDefinition& Prop,
		const FInteriorPCGRoom& Room, const FRect2D& RoomBounds, const FInteriorPCGGenerationOptions& Options,
		const FInteriorPCGGenerationResult& Result, const TArray<FPlacedProp>& Placed, FRandomStream& Stream,
		FTransform& OutLocalTransform)
	{
		const FVector LocalRoomCenter = Options.WorldTransform.InverseTransformPosition(Room.Center);
		FVector Location = LocalRoomCenter;
		FRotator Rotation = FRotator::ZeroRotator;
		const FVector2D Half = Prop.FootprintSize * 0.5;

		switch (Anchor)
		{
		case EInteriorPCGAnchorType::RoomCenter:
			break;
		case EInteriorPCGAnchorType::Wall:
		{
			const int32 Side = Stream.RandRange(0, 3);
			if (Side == 0)
			{
				Location.X = RoomBounds.Min.X + Half.X + Prop.SideClearance;
				Location.Y = Stream.FRandRange(RoomBounds.Min.Y + Half.Y, RoomBounds.Max.Y - Half.Y);
				Rotation.Yaw = 0.0;
			}
			else if (Side == 1)
			{
				Location.X = RoomBounds.Max.X - Half.X - Prop.SideClearance;
				Location.Y = Stream.FRandRange(RoomBounds.Min.Y + Half.Y, RoomBounds.Max.Y - Half.Y);
				Rotation.Yaw = 180.0;
			}
			else if (Side == 2)
			{
				Location.X = Stream.FRandRange(RoomBounds.Min.X + Half.X, RoomBounds.Max.X - Half.X);
				Location.Y = RoomBounds.Min.Y + Half.X + Prop.SideClearance;
				Rotation.Yaw = 90.0;
			}
			else
			{
				Location.X = Stream.FRandRange(RoomBounds.Min.X + Half.X, RoomBounds.Max.X - Half.X);
				Location.Y = RoomBounds.Max.Y - Half.X - Prop.SideClearance;
				Rotation.Yaw = -90.0;
			}
			break;
		}
		case EInteriorPCGAnchorType::ReferenceProp:
		{
			const FPlacedProp* Reference = Placed.FindByPredicate([&Prop](const FPlacedProp& Existing)
			{
				return Existing.Placement.PropType == Prop.ReferencePropType;
			});
			if (!Reference) return false;
			Location = Options.WorldTransform.InverseTransformPosition(Reference->Placement.Transform.GetLocation());
			Rotation = Reference->Placement.Transform.GetRelativeTransform(Options.WorldTransform).Rotator();
			break;
		}
		case EInteriorPCGAnchorType::Window:
		case EInteriorPCGAnchorType::Entrance:
		case EInteriorPCGAnchorType::Corridor:
		{
			const EInteriorPCGModuleType PortalType = Anchor == EInteriorPCGAnchorType::Window
				? EInteriorPCGModuleType::Window : EInteriorPCGModuleType::Door;
			const FInteriorPCGPortal* Portal = FindPortal(Result, Room.RoomID, PortalType);
			if (!Portal) return false;
			Location = Options.WorldTransform.InverseTransformPosition(Portal->Transform.GetLocation());
			const FVector LocalInward = Options.WorldTransform.InverseTransformVectorNoScale(Portal->InwardDirection).GetSafeNormal();
			Location += LocalInward * (Half.X + Prop.FrontClearance);
			Rotation = FRotationMatrix::MakeFromX(LocalInward).Rotator();
			break;
		}
		case EInteriorPCGAnchorType::FloorSurface:
		case EInteriorPCGAnchorType::Free:
		default:
			Location.X = Stream.FRandRange(RoomBounds.Min.X + Half.X, RoomBounds.Max.X - Half.X);
			Location.Y = Stream.FRandRange(RoomBounds.Min.Y + Half.Y, RoomBounds.Max.Y - Half.Y);
			Rotation.Yaw = static_cast<float>(Stream.RandRange(0, 3) * 90);
			break;
		}

		OutLocalTransform = FTransform(Rotation, Location);
		return true;
	}

	void ApplyLookAt(const EInteriorPCGLookAtMode LookAt, const FInteriorPCGPropDefinition& Prop,
		const FInteriorPCGRoom& Room, const FInteriorPCGGenerationOptions& Options, const FInteriorPCGGenerationResult& Result,
		const TArray<FPlacedProp>& Placed, FTransform& InOutLocalTransform)
	{
		FVector Target = InOutLocalTransform.GetLocation();
		bool bHasTarget = false;
		if (LookAt == EInteriorPCGLookAtMode::RoomCenter)
		{
			Target = Options.WorldTransform.InverseTransformPosition(Room.Center);
			bHasTarget = true;
		}
		else if (LookAt == EInteriorPCGLookAtMode::ReferenceProp)
		{
			if (const FPlacedProp* Reference = Placed.FindByPredicate([&Prop](const FPlacedProp& Existing)
			{
				return Existing.Placement.PropType == Prop.ReferencePropType;
			}))
			{
				Target = Options.WorldTransform.InverseTransformPosition(Reference->Placement.Transform.GetLocation());
				bHasTarget = true;
			}
		}
		else if (LookAt == EInteriorPCGLookAtMode::Window || LookAt == EInteriorPCGLookAtMode::Entrance || LookAt == EInteriorPCGLookAtMode::Corridor)
		{
			const EInteriorPCGModuleType PortalType = LookAt == EInteriorPCGLookAtMode::Window
				? EInteriorPCGModuleType::Window : EInteriorPCGModuleType::Door;
			if (const FInteriorPCGPortal* Portal = FindPortal(Result, Room.RoomID, PortalType))
			{
				Target = Options.WorldTransform.InverseTransformPosition(Portal->Transform.GetLocation());
				bHasTarget = true;
			}
		}

		if (bHasTarget)
		{
			FVector Direction = Target - InOutLocalTransform.GetLocation();
			Direction.Z = 0.0;
			if (!Direction.IsNearlyZero()) InOutLocalTransform.SetRotation(FRotationMatrix::MakeFromX(Direction).ToQuat());
		}
	}

	bool PlaceFunctionalSet(const FInteriorPCGFunctionalSetDefinition& Set, const FInteriorPCGRoom& Room,
		const UInteriorPCGGenerationProfile* Profile, const FInteriorPCGGenerationOptions& Options,
		FInteriorPCGGenerationResult& OutResult, const FRect2D& RoomBounds, const TArray<FRect2D>& Reserved,
		FRandomStream& Stream)
	{
		TArray<FPlacedProp> Tentative;
		for (int32 MemberIndex = 0; MemberIndex < Set.Members.Num(); ++MemberIndex)
		{
			const FInteriorPCGFunctionalSetMember& Member = Set.Members[MemberIndex];
			if (Stream.FRand() > Member.SpawnChance)
			{
				if (Member.bRequired) return false;
				continue;
			}

			const FInteriorPCGPropDefinition* Prop = Profile->InteriorProps ? Profile->InteriorProps->FindProp(Member.PropType) : nullptr;
			if (!Prop || !ContainsRoomType(Prop->AllowedRoomTypes, Room.RoomType))
			{
				if (Member.bRequired) return false;
				continue;
			}

			const EInteriorPCGAnchorType Anchor = Member.AnchorOverride != EInteriorPCGAnchorType::Free
				? Member.AnchorOverride : (MemberIndex == 0 && Prop->AnchorType == EInteriorPCGAnchorType::Free
					? EInteriorPCGAnchorType::RoomCenter : Prop->AnchorType);
			const EInteriorPCGLookAtMode LookAt = Member.LookAtOverride != EInteriorPCGLookAtMode::KeepAssetForward
				? Member.LookAtOverride : Prop->LookAtMode;
			bool bPlaced = false;
			for (int32 Attempt = 0; Attempt < Profile->InteriorRules->PlacementAttempts; ++Attempt)
			{
				FTransform LocalTransform;
				if (!ResolveAnchorTransform(Anchor, *Prop, Room, RoomBounds, Options, OutResult, Tentative, Stream, LocalTransform))
				{
					break;
				}
				LocalTransform = Member.RelativeTransform * LocalTransform;
				ApplyLookAt(LookAt, *Prop, Room, Options, OutResult, Tentative, LocalTransform);

				const FRect2D Clearance = MakeClearanceRect(LocalTransform.GetLocation(), Prop->FootprintSize,
					Prop->SideClearance, Prop->FrontClearance, LocalTransform.Rotator().Yaw);
				const bool bIgnorePath = Anchor == EInteriorPCGAnchorType::RoomCenter && MemberIndex == 0;
				if (!IsPlacementClear(Clearance, RoomBounds, Reserved, Tentative, bIgnorePath) ||
					!MeetsPortalDistances(LocalTransform.GetLocation(), *Prop, Room, Options, OutResult)) continue;

				FPlacedProp& Placed = Tentative.Emplace_GetRef();
				Placed.Clearance = Clearance;
				Placed.Placement.Kind = EInteriorPCGPlacementKind::Interior;
				Placed.Placement.PropType = Member.PropType;
				Placed.Placement.RoomType = Room.RoomType;
				Placed.Placement.AnchorType = Anchor;
				Placed.Placement.FloorIndex = Room.FloorIndex;
				Placed.Placement.RoomID = Room.RoomID;
				Placed.Placement.SetID = Set.SetID;
				Placed.Placement.BoundsExtent = FVector(Prop->FootprintSize.X * 0.5, Prop->FootprintSize.Y * 0.5, 50.0);
				Placed.Placement.Seed = DeriveSeed(Room.Seeds.RoomSeed, MemberIndex, Attempt);
				FRandomStream VariantStream(Placed.Placement.Seed);
				const FInteriorPCGAssetVariant* Variant = PickVariant(Prop->Variants, VariantStream);
				ApplyVariant(Placed.Placement, Variant, LocalTransform, Options.WorldTransform, VariantStream);
				bPlaced = true;
				break;
			}

			if (!bPlaced && Member.bRequired)
			{
				return false;
			}
		}

		for (FPlacedProp& Prop : Tentative)
		{
			OutResult.Placements.Add(MoveTemp(Prop.Placement));
		}
		return !Tentative.IsEmpty() || Set.Members.IsEmpty();
	}

	void EmitDetails(const FInteriorPCGRoom& Room, const UInteriorPCGGenerationProfile* Profile,
		const FInteriorPCGGenerationOptions& Options, FInteriorPCGGenerationResult& OutResult,
		const FRect2D& RoomBounds, const TArray<FRect2D>& Reserved)
	{
		if (!Profile->InteriorProps || !Profile->InteriorRules) return;
		FRandomStream Stream(Room.Seeds.DetailSeed);
		TArray<FPlacedProp> Placed;
		for (const FInteriorPCGPlacement& Existing : OutResult.Placements)
		{
			if (Existing.RoomID != Room.RoomID || Existing.Kind == EInteriorPCGPlacementKind::Structure) continue;
			FPlacedProp& Item = Placed.Emplace_GetRef();
			Item.Placement = Existing;
			const FVector LocalLocation = Options.WorldTransform.InverseTransformPosition(Existing.Transform.GetLocation());
			Item.Clearance = {FVector2D(LocalLocation.X - Existing.BoundsExtent.X, LocalLocation.Y - Existing.BoundsExtent.Y),
				FVector2D(LocalLocation.X + Existing.BoundsExtent.X, LocalLocation.Y + Existing.BoundsExtent.Y)};
		}

		for (const FInteriorPCGPropDefinition& Prop : Profile->InteriorProps->Props)
		{
			if (Prop.PropType != EInteriorPCGPropType::Decoration && Prop.PropType != EInteriorPCGPropType::Clutter) continue;
			if (!ContainsRoomType(Prop.AllowedRoomTypes, Room.RoomType) || Stream.FRand() > Profile->InteriorRules->DetailSpawnChance) continue;

			for (int32 Attempt = 0; Attempt < Profile->InteriorRules->PlacementAttempts; ++Attempt)
			{
				FTransform LocalTransform;
				if (!ResolveAnchorTransform(Prop.AnchorType, Prop, Room, RoomBounds, Options, OutResult, Placed, Stream, LocalTransform)) break;
				ApplyLookAt(Prop.LookAtMode, Prop, Room, Options, OutResult, Placed, LocalTransform);
				const FRect2D Clearance = MakeClearanceRect(LocalTransform.GetLocation(), Prop.FootprintSize,
					Prop.SideClearance, Prop.FrontClearance, LocalTransform.Rotator().Yaw);
				if (!IsPlacementClear(Clearance, RoomBounds, Reserved, Placed, false) ||
					!MeetsPortalDistances(LocalTransform.GetLocation(), Prop, Room, Options, OutResult)) continue;

				FPlacedProp& Item = Placed.Emplace_GetRef();
				Item.Clearance = Clearance;
				Item.Placement.Kind = EInteriorPCGPlacementKind::Interior;
				Item.Placement.PropType = Prop.PropType;
				Item.Placement.RoomType = Room.RoomType;
				Item.Placement.AnchorType = Prop.AnchorType;
				Item.Placement.FloorIndex = Room.FloorIndex;
				Item.Placement.RoomID = Room.RoomID;
				Item.Placement.Seed = DeriveSeed(Room.Seeds.DetailSeed, static_cast<int32>(Prop.PropType), Attempt);
				FRandomStream VariantStream(Item.Placement.Seed);
				const FInteriorPCGAssetVariant* Variant = PickVariant(Prop.Variants, VariantStream);
				ApplyVariant(Item.Placement, Variant, LocalTransform, Options.WorldTransform, VariantStream);
				OutResult.Placements.Add(Item.Placement);
				break;
			}
		}
	}

	void EmitInteriors(FInteriorPCGGenerationResult& OutResult, const UInteriorPCGGenerationProfile* Profile,
		const FInteriorPCGGenerationOptions& Options)
	{
		if (!Profile->InteriorRules || !Profile->InteriorProps)
		{
			OutResult.Warnings.Add(TEXT("Interior generation was requested without both an Interior Rule Set and Interior Prop Set."));
			return;
		}

		const double CellSize = Profile->BuildingRules->CellSize;
		const int32 Columns = FMath::Max(1, FMath::FloorToInt(Options.Footprint.X / CellSize));
		const int32 Rows = FMath::Max(1, FMath::FloorToInt(Options.Footprint.Y / CellSize));
		const FVector GridOrigin(-Columns * CellSize * 0.5, -Rows * CellSize * 0.5, 0.0);

		for (const FInteriorPCGRoom& Room : OutResult.Rooms)
		{
			if (Room.RoomType == EInteriorPCGRoomType::Corridor || Room.RoomType == EInteriorPCGRoomType::Undefined) continue;
			const double Margin = 20.0;
			const FRect2D RoomBounds{
				FVector2D(GridOrigin.X + Room.GridMin.X * CellSize + Margin, GridOrigin.Y + Room.GridMin.Y * CellSize + Margin),
				FVector2D(GridOrigin.X + Room.GridMax.X * CellSize - Margin, GridOrigin.Y + Room.GridMax.Y * CellSize - Margin)};
			TArray<FRect2D> Reserved;
			const FVector LocalCenter = Options.WorldTransform.InverseTransformPosition(Room.Center);

			for (const FInteriorPCGPortal& Portal : OutResult.Portals)
			{
				if (Portal.RoomID != Room.RoomID) continue;
				const FVector LocalPortal = Options.WorldTransform.InverseTransformPosition(Portal.Transform.GetLocation());
				const FVector2D PortalPoint(LocalPortal.X, LocalPortal.Y);
				const FVector LocalInward = Options.WorldTransform.InverseTransformVectorNoScale(Portal.InwardDirection).GetSafeNormal();
				if (Portal.ModuleType == EInteriorPCGModuleType::Door)
				{
					const FVector2D Start = PortalPoint + FVector2D(LocalInward.X, LocalInward.Y) * 20.0;
					const FVector2D End = FMath::Lerp(Start, FVector2D(LocalCenter.X, LocalCenter.Y), 0.7);
					Reserved.Add(MakeSegmentRect(Start, End, Profile->InteriorRules->MainPathWidth * 0.5));
					Reserved.Add(MakeSegmentRect(PortalPoint, Start + FVector2D(LocalInward.X, LocalInward.Y) * Profile->InteriorRules->DoorApproachDepth,
						Profile->InteriorRules->MainPathWidth * 0.5));
				}
				else if (Portal.ModuleType == EInteriorPCGModuleType::Window)
				{
					Reserved.Add(MakeSegmentRect(PortalPoint, PortalPoint + FVector2D(LocalInward.X, LocalInward.Y) *
						Profile->InteriorRules->WindowApproachDepth, Profile->InteriorRules->MainPathWidth * 0.25));
				}
			}

			FRandomStream Stream(Room.Seeds.RoomSeed);
			if (const FInteriorPCGFunctionalSetDefinition* Set = PickFunctionalSet(Room, *Profile->InteriorRules, Stream))
			{
				if (!PlaceFunctionalSet(*Set, Room, Profile, Options, OutResult, RoomBounds, Reserved, Stream))
				{
					OutResult.Warnings.Add(FString::Printf(TEXT("Functional set '%s' did not fit room %d; the set was rolled back."),
						*Set->SetID.ToString(), Room.RoomID));
				}
			}
			EmitDetails(Room, Profile, Options, OutResult, RoomBounds, Reserved);
		}
	}

	int32 ComputeLayoutHash(const FInteriorPCGGenerationResult& Result)
	{
		uint32 Hash = GetTypeHash(Result.Rooms.Num());
		for (const FInteriorPCGRoom& Room : Result.Rooms)
		{
			Hash = HashCombineFast(Hash, GetTypeHash(Room.RoomID));
			Hash = HashCombineFast(Hash, GetTypeHash(static_cast<uint8>(Room.RoomType)));
			Hash = HashCombineFast(Hash, GetTypeHash(Room.GridMin));
			Hash = HashCombineFast(Hash, GetTypeHash(Room.GridMax));
		}
		for (const FInteriorPCGPlacement& Placement : Result.Placements)
		{
			Hash = HashCombineFast(Hash, GetTypeHash(static_cast<uint8>(Placement.ModuleType)));
			Hash = HashCombineFast(Hash, GetTypeHash(static_cast<uint8>(Placement.PropType)));
			Hash = HashCombineFast(Hash, GetTypeHash(Placement.Transform.GetLocation()));
			Hash = HashCombineFast(Hash, GetTypeHash(Placement.Seed));
		}
		return static_cast<int32>(Hash & 0x7fffffffu);
	}
}

bool UInteriorPCGGenerationLibrary::ValidateProfile(const UInteriorPCGGenerationProfile* Profile, TArray<FString>& OutErrors)
{
	using namespace InteriorPCG::Private;
	OutErrors.Reset();
	if (!Profile)
	{
		OutErrors.Add(TEXT("Generation Profile is null."));
		return false;
	}
	if (!Profile->BuildingRules)
	{
		OutErrors.Add(TEXT("Building Rule Set is required."));
		return false;
	}
	if (Profile->BuildingRules->CellSize < 100.0)
	{
		OutErrors.Add(TEXT("Building Rule Set CellSize must be at least 100 cm."));
	}
	if (Profile->BuildingRules->FloorHeight < 100.0)
	{
		OutErrors.Add(TEXT("Building Rule Set FloorHeight must be at least 100 cm."));
	}
	for (const FInteriorPCGVerticalCoreDefinition& Core : Profile->BuildingRules->VerticalCores)
	{
		if (Core.ModuleType != EInteriorPCGModuleType::Stair && Core.ModuleType != EInteriorPCGModuleType::Elevator &&
			Core.ModuleType != EInteriorPCGModuleType::ServiceShaft)
		{
			OutErrors.Add(FString::Printf(TEXT("Vertical core '%s' must use Stair, Elevator, or ServiceShaft."), *Core.CoreID.ToString()));
		}
	}
	if (Profile->BuildingModules)
	{
		TArray<EInteriorPCGModuleType, TInlineAllocator<8>> Required = {EInteriorPCGModuleType::Floor,
			EInteriorPCGModuleType::ExteriorWall, EInteriorPCGModuleType::InteriorWall, EInteriorPCGModuleType::Door};
		if (Profile->BuildingRules->bGenerateCeilingTiles) Required.Add(EInteriorPCGModuleType::Ceiling);
		if (Profile->BuildingRules->bGenerateRoofTiles) Required.Add(EInteriorPCGModuleType::Roof);
		if (Profile->BuildingRules->RoofDecorationCount > 0) Required.Add(EInteriorPCGModuleType::RoofDecoration);
		for (const EInteriorPCGModuleType Type : Required)
		{
			const bool bHasModule = Profile->BuildingModules->Modules.ContainsByPredicate([Type](const FInteriorPCGBuildingModuleDefinition& Module)
			{
				return Module.ModuleType == Type && !Module.Variants.IsEmpty();
			});
			if (!bHasModule)
			{
				OutErrors.Add(FString::Printf(TEXT("Building Module Set has no %s variants; semantic signals will still be emitted."), *EnumName(Type)));
			}
		}
	}
	if (Profile->InteriorRules && Profile->InteriorProps)
	{
		for (const FInteriorPCGFunctionalSetDefinition& Set : Profile->InteriorRules->FunctionalSets)
		{
			for (const FInteriorPCGFunctionalSetMember& Member : Set.Members)
			{
				if (!Profile->InteriorProps->FindProp(Member.PropType))
				{
					OutErrors.Add(FString::Printf(TEXT("Functional set '%s' references a missing prop type."), *Set.SetID.ToString()));
				}
			}
		}
	}
	return !OutErrors.ContainsByPredicate([](const FString& Error)
	{
		return Error.Contains(TEXT("required")) || Error.Contains(TEXT("must")) || Error.Contains(TEXT("missing prop"));
	});
}

FInteriorPCGSeedBundle UInteriorPCGGenerationLibrary::MakeSeedBundle(const int32 BuildingSeed, const int32 FloorIndex, const int32 RoomID)
{
	using namespace InteriorPCG::Private;
	FInteriorPCGSeedBundle Seeds;
	Seeds.BuildingSeed = BuildingSeed;
	Seeds.FloorSeed = DeriveSeed(BuildingSeed, FloorIndex, 101);
	Seeds.RoomSeed = DeriveSeed(Seeds.FloorSeed, RoomID, 211);
	Seeds.DetailSeed = DeriveSeed(Seeds.RoomSeed, RoomID, 307);
	return Seeds;
}

bool UInteriorPCGGenerationLibrary::Generate(const UInteriorPCGGenerationProfile* Profile,
	const FInteriorPCGGenerationOptions& Options, FInteriorPCGGenerationResult& OutResult)
{
	using namespace InteriorPCG::Private;
	OutResult.Reset();
	TArray<FString> ValidationMessages;
	if (!ValidateProfile(Profile, ValidationMessages))
	{
		OutResult.Warnings = MoveTemp(ValidationMessages);
		return false;
	}
	OutResult.Warnings = MoveTemp(ValidationMessages);

	const UInteriorPCGBuildingRuleSet& Rules = *Profile->BuildingRules;
	const int32 Columns = FMath::FloorToInt(Options.Footprint.X / Rules.CellSize);
	const int32 Rows = FMath::FloorToInt(Options.Footprint.Y / Rules.CellSize);
	if (Columns < 2 || Rows < 2 || Options.NumFloors < 1)
	{
		OutResult.Warnings.Add(TEXT("Footprint must contain at least 2 x 2 cells and NumFloors must be positive."));
		return false;
	}

	const TArray<FResolvedCore> ResolvedCores = ResolveCores(Rules, Columns, Rows);
	for (int32 FloorIndex = 0; FloorIndex < Options.NumFloors; ++FloorIndex)
	{
		FFloorState State;
		State.FloorIndex = FloorIndex;
		State.Columns = Columns;
		State.Rows = Rows;
		State.bHorizontalCorridor = Columns >= Rows;
		State.Owners.Init(EmptyCell, Columns * Rows);
		State.Cores = ResolvedCores;

		int32 VariationKey = FloorIndex;
		if (FloorIndex > 0 && Rules.RepeatFloorVariation == EInteriorPCGFloorVariationMode::Identical)
		{
			VariationKey = 1;
		}
		else if (FloorIndex > 0 && Rules.RepeatFloorVariation == EInteriorPCGFloorVariationMode::PatternCycle)
		{
			VariationKey = 1 + ((FloorIndex - 1) % FMath::Max(1, Rules.PatternLength));
		}
		State.FloorSeed = DeriveSeed(Options.BuildingSeed, VariationKey, 101);

		MarkCorridor(State, Rules);
		const int32 FirstRoomIndex = OutResult.Rooms.Num();
		PartitionRooms(State, OutResult, Rules, Options);
		ClassifyRooms(State, OutResult, Rules, Options, FirstRoomIndex);
		AddPortals(State, OutResult, Rules, Options, FirstRoomIndex);
		if (Options.bGenerateStructure)
		{
			EmitStructure(State, OutResult, Profile, Options);
		}
	}

	if (Options.bGenerateInteriors)
	{
		EmitInteriors(OutResult, Profile, Options);
	}
	OutResult.LayoutHash = ComputeLayoutHash(OutResult);
	OutResult.bSucceeded = true;
	return true;
}
