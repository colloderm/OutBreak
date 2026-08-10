// Copyright OutBreak. All Rights Reserved.

#include "PCGInteriorLayoutSettings.h"

#include "InteriorPCGDataAssets.h"
#include "InteriorPCGGenerationLibrary.h"
#include "PCGContext.h"
#include "PCGPoint.h"
#include "Data/PCGPointData.h"
#include "Metadata/PCGMetadata.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGInteriorLayoutSettings)

#define LOCTEXT_NAMESPACE "PCGInteriorLayoutElement"

namespace InteriorPCG::PCGPrivate
{
	FName EnumName(const UEnum* Enum, const int64 Value)
	{
		return Enum ? FName(*Enum->GetNameStringByValue(Value)) : NAME_None;
	}

	UPCGPointData* MakePlacementPointData(FPCGContext* Context, const TArray<FInteriorPCGPlacement>& Placements,
		const EInteriorPCGPlacementKind IncludedKind, const bool bIncludeInteractive)
	{
		UPCGPointData* PointData = FPCGContext::NewObject_AnyThread<UPCGPointData>(Context);
		FPCGMetadataAttribute<FName>* KindAttribute = PointData->Metadata->CreateAttribute<FName>(TEXT("InteriorPCG.Kind"), NAME_None, false, false);
		FPCGMetadataAttribute<FName>* ModuleAttribute = PointData->Metadata->CreateAttribute<FName>(TEXT("InteriorPCG.ModuleType"), NAME_None, false, false);
		FPCGMetadataAttribute<FName>* PropAttribute = PointData->Metadata->CreateAttribute<FName>(TEXT("InteriorPCG.PropType"), NAME_None, false, false);
		FPCGMetadataAttribute<FName>* RoomTypeAttribute = PointData->Metadata->CreateAttribute<FName>(TEXT("InteriorPCG.RoomType"), NAME_None, false, false);
		FPCGMetadataAttribute<FName>* AnchorAttribute = PointData->Metadata->CreateAttribute<FName>(TEXT("InteriorPCG.AnchorType"), NAME_None, false, false);
		FPCGMetadataAttribute<FName>* FloorBandAttribute = PointData->Metadata->CreateAttribute<FName>(TEXT("InteriorPCG.FloorBand"), NAME_None, false, false);
		FPCGMetadataAttribute<FName>* SetAttribute = PointData->Metadata->CreateAttribute<FName>(TEXT("InteriorPCG.SetID"), NAME_None, false, false);
		FPCGMetadataAttribute<FName>* VariantAttribute = PointData->Metadata->CreateAttribute<FName>(TEXT("InteriorPCG.VariantID"), NAME_None, false, false);
		FPCGMetadataAttribute<int32>* FloorAttribute = PointData->Metadata->CreateAttribute<int32>(TEXT("InteriorPCG.FloorIndex"), 0, false, false);
		FPCGMetadataAttribute<int32>* RoomAttribute = PointData->Metadata->CreateAttribute<int32>(TEXT("InteriorPCG.RoomID"), INDEX_NONE, false, false);
		FPCGMetadataAttribute<FSoftObjectPath>* AssetAttribute = PointData->Metadata->CreateAttribute<FSoftObjectPath>(TEXT("InteriorPCG.AssetPath"), FSoftObjectPath(), false, false);
		FPCGMetadataAttribute<FSoftObjectPath>* ActorAttribute = PointData->Metadata->CreateAttribute<FSoftObjectPath>(TEXT("InteriorPCG.ActorClassPath"), FSoftObjectPath(), false, false);
		FPCGMetadataAttribute<bool>* InstancedAttribute = PointData->Metadata->CreateAttribute<bool>(TEXT("InteriorPCG.AllowInstancing"), true, false, false);
		FPCGMetadataAttribute<bool>* InteractiveAttribute = PointData->Metadata->CreateAttribute<bool>(TEXT("InteriorPCG.Interactive"), false, false, false);

		TArray<FPCGPoint>& Points = PointData->GetMutablePoints();
		for (const FInteriorPCGPlacement& Placement : Placements)
		{
			const bool bMatches = Placement.Kind == IncludedKind || (bIncludeInteractive && Placement.Kind == EInteriorPCGPlacementKind::InteractiveActor);
			if (!bMatches) continue;

			TArray<UStaticMesh*, TInlineAllocator<3>> MeshLayers;
			if (Placement.StaticMesh) MeshLayers.Add(Placement.StaticMesh);
			for (UStaticMesh* AdditionalMesh : Placement.AdditionalStaticMeshes)
			{
				if (AdditionalMesh) MeshLayers.Add(AdditionalMesh);
			}
			if (MeshLayers.IsEmpty()) MeshLayers.Add(nullptr);

			for (int32 LayerIndex = 0; LayerIndex < MeshLayers.Num(); ++LayerIndex)
			{
				FPCGPoint& Point = Points.Emplace_GetRef();
				Point.Transform = Placement.Transform;
				Point.SetExtents(Placement.BoundsExtent);
				Point.Density = 1.0f;
				Point.Steepness = 1.0f;
				Point.Seed = HashCombineFast(Placement.Seed, LayerIndex);
				Point.MetadataEntry = PointData->Metadata->AddEntry();

				KindAttribute->SetValue(Point.MetadataEntry, EnumName(StaticEnum<EInteriorPCGPlacementKind>(), static_cast<int64>(Placement.Kind)));
				ModuleAttribute->SetValue(Point.MetadataEntry, EnumName(StaticEnum<EInteriorPCGModuleType>(), static_cast<int64>(Placement.ModuleType)));
				PropAttribute->SetValue(Point.MetadataEntry, EnumName(StaticEnum<EInteriorPCGPropType>(), static_cast<int64>(Placement.PropType)));
				RoomTypeAttribute->SetValue(Point.MetadataEntry, EnumName(StaticEnum<EInteriorPCGRoomType>(), static_cast<int64>(Placement.RoomType)));
				AnchorAttribute->SetValue(Point.MetadataEntry, EnumName(StaticEnum<EInteriorPCGAnchorType>(), static_cast<int64>(Placement.AnchorType)));
				FloorBandAttribute->SetValue(Point.MetadataEntry, EnumName(StaticEnum<EInteriorPCGFloorBand>(), static_cast<int64>(Placement.FloorBand)));
				SetAttribute->SetValue(Point.MetadataEntry, Placement.SetID);
				VariantAttribute->SetValue(Point.MetadataEntry, Placement.VariantID);
				FloorAttribute->SetValue(Point.MetadataEntry, Placement.FloorIndex);
				RoomAttribute->SetValue(Point.MetadataEntry, Placement.RoomID);
				AssetAttribute->SetValue(Point.MetadataEntry, MeshLayers[LayerIndex] ? FSoftObjectPath(MeshLayers[LayerIndex]) : FSoftObjectPath());
				// Actor placement is carried only by the first mesh layer so downstream spawners do not duplicate it.
				ActorAttribute->SetValue(Point.MetadataEntry, LayerIndex == 0 && Placement.ActorClass ? FSoftObjectPath(Placement.ActorClass.Get()) : FSoftObjectPath());
				InstancedAttribute->SetValue(Point.MetadataEntry, Placement.bAllowInstancing);
				InteractiveAttribute->SetValue(Point.MetadataEntry, Placement.bInteractive);
			}
		}
		return PointData;
	}

	UPCGPointData* MakeRoomPointData(FPCGContext* Context, const TArray<FInteriorPCGRoom>& Rooms)
	{
		UPCGPointData* PointData = FPCGContext::NewObject_AnyThread<UPCGPointData>(Context);
		FPCGMetadataAttribute<int32>* FloorAttribute = PointData->Metadata->CreateAttribute<int32>(TEXT("InteriorPCG.FloorIndex"), 0, false, false);
		FPCGMetadataAttribute<int32>* RoomAttribute = PointData->Metadata->CreateAttribute<int32>(TEXT("InteriorPCG.RoomID"), INDEX_NONE, false, false);
		FPCGMetadataAttribute<FName>* RoomTypeAttribute = PointData->Metadata->CreateAttribute<FName>(TEXT("InteriorPCG.RoomType"), NAME_None, false, false);
		FPCGMetadataAttribute<double>* AreaAttribute = PointData->Metadata->CreateAttribute<double>(TEXT("InteriorPCG.Area"), 0.0, false, false);
		FPCGMetadataAttribute<bool>* ExteriorAttribute = PointData->Metadata->CreateAttribute<bool>(TEXT("InteriorPCG.TouchesExterior"), false, false, false);
		FPCGMetadataAttribute<bool>* ConnectedAttribute = PointData->Metadata->CreateAttribute<bool>(TEXT("InteriorPCG.ConnectedToCorridor"), false, false, false);

		TArray<FPCGPoint>& Points = PointData->GetMutablePoints();
		for (const FInteriorPCGRoom& Room : Rooms)
		{
			FPCGPoint& Point = Points.Emplace_GetRef();
			Point.Transform = FTransform(FRotator::ZeroRotator, Room.Center);
			Point.SetExtents(Room.Extents);
			Point.Density = 1.0f;
			Point.Steepness = 1.0f;
			Point.Seed = Room.Seeds.RoomSeed;
			Point.MetadataEntry = PointData->Metadata->AddEntry();
			FloorAttribute->SetValue(Point.MetadataEntry, Room.FloorIndex);
			RoomAttribute->SetValue(Point.MetadataEntry, Room.RoomID);
			RoomTypeAttribute->SetValue(Point.MetadataEntry, EnumName(StaticEnum<EInteriorPCGRoomType>(), static_cast<int64>(Room.RoomType)));
			AreaAttribute->SetValue(Point.MetadataEntry, Room.Area);
			ExteriorAttribute->SetValue(Point.MetadataEntry, Room.bTouchesExterior);
			ConnectedAttribute->SetValue(Point.MetadataEntry, Room.bConnectedToCorridor);
		}
		return PointData;
	}

	void AddOutput(FPCGContext* Context, const FName Pin, UPCGData* Data)
	{
		FPCGTaggedData& TaggedData = Context->OutputData.TaggedData.Emplace_GetRef();
		TaggedData.Pin = Pin;
		TaggedData.Data = Data;
	}
}

TArray<FPCGPinProperties> UPCGInteriorLayoutSettings::InputPinProperties() const
{
	return {};
}

TArray<FPCGPinProperties> UPCGInteriorLayoutSettings::OutputPinProperties() const
{
	return {
		FPCGPinProperties(InteriorPCGPinConstants::Structure, FPCGDataTypeIdentifier{EPCGDataType::Point}, false, false),
		FPCGPinProperties(InteriorPCGPinConstants::Interior, FPCGDataTypeIdentifier{EPCGDataType::Point}, false, false),
		FPCGPinProperties(InteriorPCGPinConstants::Rooms, FPCGDataTypeIdentifier{EPCGDataType::Point}, false, false)
	};
}

FPCGElementPtr UPCGInteriorLayoutSettings::CreateElement() const
{
	return MakeShared<FPCGInteriorLayoutElement>();
}

bool FPCGInteriorLayoutElement::ExecuteInternal(FPCGContext* Context) const
{
	check(Context);
	const UPCGInteriorLayoutSettings* Settings = Context->GetInputSettings<UPCGInteriorLayoutSettings>();
	if (!Settings || !Settings->Profile)
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("MissingProfile", "Interior PCG Generation Profile is required."));
		return true;
	}

	FInteriorPCGGenerationOptions Options = Settings->GenerationOptions;
	if (Context->ExecutionSource.IsValid())
	{
		Options.WorldTransform = Context->ExecutionSource->GetExecutionState().GetTransform();
	}

	FInteriorPCGGenerationResult Result;
	if (!UInteriorPCGGenerationLibrary::Generate(Settings->Profile, Options, Result))
	{
		for (const FString& Warning : Result.Warnings)
		{
			PCGE_LOG(Error, GraphAndLog, FText::FromString(Warning));
		}
		return true;
	}

	for (const FString& Warning : Result.Warnings)
	{
		PCGE_LOG(Warning, GraphAndLog, FText::FromString(Warning));
	}

	using namespace InteriorPCG::PCGPrivate;
	AddOutput(Context, InteriorPCGPinConstants::Structure,
		MakePlacementPointData(Context, Result.Placements, EInteriorPCGPlacementKind::Structure, false));
	AddOutput(Context, InteriorPCGPinConstants::Interior,
		MakePlacementPointData(Context, Result.Placements, EInteriorPCGPlacementKind::Interior, true));
	AddOutput(Context, InteriorPCGPinConstants::Rooms, MakeRoomPointData(Context, Result.Rooms));
	return true;
}

#undef LOCTEXT_NAMESPACE
