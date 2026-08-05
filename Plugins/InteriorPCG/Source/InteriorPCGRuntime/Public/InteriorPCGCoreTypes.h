// Copyright OutBreak. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "InteriorPCGCoreTypes.generated.h"

class AActor;
class UStaticMesh;

UENUM(BlueprintType)
enum class EInteriorPCGModuleType : uint8
{
	None,
	Floor,
	FloorOpening,
	Ceiling,
	Roof,
	ExteriorWall,
	ExteriorCorner,
	InteriorWall,
	Door,
	Window,
	Column,
	Stair,
	Elevator,
	ServiceShaft,
	RoofDecoration
};

/** Vertical facade context. Asset packs commonly provide different ground, repeat, and top modules. */
UENUM(BlueprintType)
enum class EInteriorPCGFloorBand : uint8
{
	Any,
	Ground,
	Middle,
	Top,
	Roof
};

UENUM(BlueprintType)
enum class EInteriorPCGRoomType : uint8
{
	Undefined,
	Lobby,
	Corridor,
	Security,
	Management,
	Office,
	Meeting,
	Lounge,
	Workshop,
	Storage,
	Utility,
	Kitchen,
	Bedroom,
	Living,
	Commercial
};

UENUM(BlueprintType)
enum class EInteriorPCGPropType : uint8
{
	None,
	Desk,
	Chair,
	Table,
	Sofa,
	Shelf,
	Cabinet,
	Monitor,
	Television,
	Bed,
	Counter,
	Appliance,
	Decoration,
	Clutter
};

UENUM(BlueprintType)
enum class EInteriorPCGAnchorType : uint8
{
	Free,
	RoomCenter,
	Wall,
	FloorSurface,
	ReferenceProp,
	Window,
	Entrance,
	Corridor
};

UENUM(BlueprintType)
enum class EInteriorPCGLookAtMode : uint8
{
	KeepAssetForward,
	RoomCenter,
	Wall,
	ReferenceProp,
	Window,
	Entrance,
	Corridor
};

UENUM(BlueprintType)
enum class EInteriorPCGPlacementKind : uint8
{
	Structure,
	Interior,
	InteractiveActor
};

UENUM(BlueprintType)
enum class EInteriorPCGFloorVariationMode : uint8
{
	Identical,
	SeededPerFloor,
	PatternCycle
};

UENUM(BlueprintType)
enum class EInteriorPCGEntranceSide : uint8
{
	South,
	North,
	West,
	East
};

/** A mesh or actor that can satisfy a semantic building/prop request. */
USTRUCT(BlueprintType)
struct INTERIORPCGRUNTIME_API FInteriorPCGAssetVariant
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Asset")
	FName VariantID = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Asset")
	TObjectPtr<UStaticMesh> StaticMesh = nullptr;

	/** Co-located mesh layers such as a facade shell plus its window/glass insert. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Asset")
	TArray<TObjectPtr<UStaticMesh>> AdditionalStaticMeshes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Asset")
	TSubclassOf<AActor> ActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Asset", meta = (ClampMin = "0.0"))
	float Weight = 1.0f;

	/** Authored mesh size. It is data for validation and point bounds; the solver never guesses mesh bounds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Asset", meta = (Units = "cm"))
	FVector NominalSize = FVector(400.0, 20.0, 300.0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Asset")
	FTransform PlacementOffset;

	/** Extra yaw values are applied after PlacementOffset. Empty means zero degrees only. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Asset")
	TArray<float> AllowedYawDegrees;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Asset")
	bool bAllowInstancing = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Asset")
	bool bInteractive = false;
};

USTRUCT(BlueprintType)
struct INTERIORPCGRUNTIME_API FInteriorPCGGenerationOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation", meta = (ClampMin = "100.0", Units = "cm"))
	FVector2D Footprint = FVector2D(4000.0, 3000.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation", meta = (ClampMin = "1", ClampMax = "128"))
	int32 NumFloors = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
	int32 BuildingSeed = 1337;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
	FTransform WorldTransform;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
	bool bGenerateStructure = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
	bool bGenerateInteriors = true;
};

USTRUCT(BlueprintType)
struct INTERIORPCGRUNTIME_API FInteriorPCGSeedBundle
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Seed")
	int32 BuildingSeed = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Seed")
	int32 FloorSeed = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Seed")
	int32 RoomSeed = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Seed")
	int32 DetailSeed = 0;
};

/** Logical room output. GridMin is inclusive and GridMax is exclusive. */
USTRUCT(BlueprintType)
struct INTERIORPCGRUNTIME_API FInteriorPCGRoom
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room")
	int32 RoomID = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room")
	int32 FloorIndex = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room")
	EInteriorPCGRoomType RoomType = EInteriorPCGRoomType::Undefined;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room")
	FIntPoint GridMin = FIntPoint::ZeroValue;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room")
	FIntPoint GridMax = FIntPoint::ZeroValue;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room")
	FVector Center = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room", meta = (Units = "cm"))
	FVector Extents = FVector::ZeroVector;

	/** Area in square centimeters. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room")
	double Area = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room")
	bool bTouchesExterior = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room")
	bool bConnectedToCorridor = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room")
	FInteriorPCGSeedBundle Seeds;
};

USTRUCT(BlueprintType)
struct INTERIORPCGRUNTIME_API FInteriorPCGPortal
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal")
	EInteriorPCGModuleType ModuleType = EInteriorPCGModuleType::Door;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal")
	int32 FloorIndex = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal")
	int32 RoomID = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal")
	FTransform Transform;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal")
	FVector InwardDirection = FVector::ForwardVector;
};

/** Final semantic signal. It is useful without an assigned asset and can be consumed by Blueprint or PCG. */
USTRUCT(BlueprintType)
struct INTERIORPCGRUNTIME_API FInteriorPCGPlacement
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Placement")
	EInteriorPCGPlacementKind Kind = EInteriorPCGPlacementKind::Structure;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Placement")
	EInteriorPCGModuleType ModuleType = EInteriorPCGModuleType::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Placement")
	EInteriorPCGPropType PropType = EInteriorPCGPropType::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Placement")
	EInteriorPCGRoomType RoomType = EInteriorPCGRoomType::Undefined;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Placement")
	EInteriorPCGAnchorType AnchorType = EInteriorPCGAnchorType::Free;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Placement")
	EInteriorPCGFloorBand FloorBand = EInteriorPCGFloorBand::Any;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Placement")
	int32 FloorIndex = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Placement")
	int32 RoomID = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Placement")
	FName SetID = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Placement")
	FName VariantID = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Placement")
	FTransform Transform;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Placement", meta = (Units = "cm"))
	FVector BoundsExtent = FVector(50.0);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Placement")
	TObjectPtr<UStaticMesh> StaticMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Placement")
	TArray<TObjectPtr<UStaticMesh>> AdditionalStaticMeshes;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Placement")
	TSubclassOf<AActor> ActorClass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Placement")
	bool bAllowInstancing = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Placement")
	bool bInteractive = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Placement")
	int32 Seed = 0;
};

USTRUCT(BlueprintType)
struct INTERIORPCGRUNTIME_API FInteriorPCGGenerationResult
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Result")
	bool bSucceeded = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Result")
	TArray<FInteriorPCGRoom> Rooms;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Result")
	TArray<FInteriorPCGPortal> Portals;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Result")
	TArray<FInteriorPCGPlacement> Placements;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Result")
	TArray<FString> Warnings;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Result")
	int32 LayoutHash = 0;

	void Reset()
	{
		*this = FInteriorPCGGenerationResult();
	}
};
