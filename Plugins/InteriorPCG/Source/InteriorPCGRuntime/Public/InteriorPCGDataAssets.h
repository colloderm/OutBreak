// Copyright OutBreak. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "InteriorPCGCoreTypes.h"
#include "InteriorPCGDataAssets.generated.h"

USTRUCT(BlueprintType)
struct INTERIORPCGRUNTIME_API FInteriorPCGWeightedRoomType
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room")
	EInteriorPCGRoomType RoomType = EInteriorPCGRoomType::Office;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room", meta = (ClampMin = "0.0"))
	float Weight = 1.0f;

	/** Minimum area in square meters. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room", meta = (ClampMin = "0.0"))
	float MinimumAreaSquareMeters = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room")
	bool bRequiresExteriorWall = false;
};

USTRUCT(BlueprintType)
struct INTERIORPCGRUNTIME_API FInteriorPCGVerticalCoreDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core")
	FName CoreID = TEXT("Core");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core")
	EInteriorPCGModuleType ModuleType = EInteriorPCGModuleType::Stair;

	/** Normalized footprint position. The same resolved cell is reused on every floor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	FVector2D NormalizedPosition = FVector2D(0.5, 0.5);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core", meta = (ClampMin = "1"))
	FIntPoint SizeInCells = FIntPoint(2, 2);
};

USTRUCT(BlueprintType)
struct INTERIORPCGRUNTIME_API FInteriorPCGExteriorEntranceDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Entrance")
	EInteriorPCGEntranceSide Side = EInteriorPCGEntranceSide::South;

	/** Position along the selected facade, from 0 at its first grid cell to 1 at its last. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Entrance", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double NormalizedPosition = 0.5;
};

USTRUCT(BlueprintType)
struct INTERIORPCGRUNTIME_API FInteriorPCGBuildingModuleDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Module")
	EInteriorPCGModuleType ModuleType = EInteriorPCGModuleType::None;

	/** Empty or Any matches every floor. More specific entries override the Any fallback. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Module")
	TArray<EInteriorPCGFloorBand> FloorBands;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Module")
	TArray<FInteriorPCGAssetVariant> Variants;
};

UCLASS(BlueprintType)
class INTERIORPCGRUNTIME_API UInteriorPCGBuildingRuleSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid", meta = (ClampMin = "100.0", Units = "cm"))
	double CellSize = 400.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid", meta = (ClampMin = "100.0", Units = "cm"))
	double FloorHeight = 350.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "1"))
	int32 CorridorWidthInCells = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "1"))
	int32 MinimumRoomLengthInCells = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "1"))
	int32 MaximumRoomLengthInCells = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	EInteriorPCGFloorVariationMode RepeatFloorVariation = EInteriorPCGFloorVariationMode::SeededPerFloor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "1"))
	int32 PatternLength = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	TArray<FInteriorPCGVerticalCoreDefinition> VerticalCores;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rooms")
	TArray<FInteriorPCGWeightedRoomType> FirstFloorRoomTypes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rooms")
	TArray<FInteriorPCGWeightedRoomType> RepeatFloorRoomTypes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Openings", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ExteriorWindowChance = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Openings", meta = (ClampMin = "1"))
	int32 MinimumWindowSpacingInCells = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Openings")
	EInteriorPCGEntranceSide MainEntranceSide = EInteriorPCGEntranceSide::South;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Openings", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double MainEntrancePosition = 0.5;

	/** Secondary exterior doors. The primary entrance still controls lobby selection. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Openings")
	TArray<FInteriorPCGExteriorEntranceDefinition> AdditionalEntrances;

	/** Disable when the floor mesh also serves as the underside/ceiling, as in Post-Apocalypse Building Bundle. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Structure")
	bool bGenerateCeilingTiles = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Structure")
	bool bGenerateRoofTiles = true;

	/** Number of deterministic semantic roof-decoration placements on the roof grid. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Structure", meta = (ClampMin = "0"))
	int32 RoofDecorationCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Structure")
	bool bUseFloorOpeningsAtVerticalCores = true;

	/** Emits one corner module at each footprint corner and reserves adjacent edge spans for it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Structure")
	bool bUseDedicatedExteriorCorners = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Structure", meta = (ClampMin = "0"))
	int32 ExteriorCornerSpanInCells = 1;
};

UCLASS(BlueprintType)
class INTERIORPCGRUNTIME_API UInteriorPCGBuildingModuleSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modules")
	TArray<FInteriorPCGBuildingModuleDefinition> Modules;

	const FInteriorPCGBuildingModuleDefinition* FindModule(EInteriorPCGModuleType ModuleType,
		EInteriorPCGFloorBand FloorBand = EInteriorPCGFloorBand::Any) const;
};

USTRUCT(BlueprintType)
struct INTERIORPCGRUNTIME_API FInteriorPCGPropDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Prop")
	EInteriorPCGPropType PropType = EInteriorPCGPropType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Prop")
	TArray<EInteriorPCGRoomType> AllowedRoomTypes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	EInteriorPCGAnchorType AnchorType = EInteriorPCGAnchorType::Free;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	EInteriorPCGLookAtMode LookAtMode = EInteriorPCGLookAtMode::KeepAssetForward;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	EInteriorPCGPropType ReferencePropType = EInteriorPCGPropType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clearance", meta = (ClampMin = "0.0", Units = "cm"))
	FVector2D FootprintSize = FVector2D(100.0, 100.0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clearance", meta = (ClampMin = "0.0", Units = "cm"))
	float SideClearance = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clearance", meta = (ClampMin = "0.0", Units = "cm"))
	float FrontClearance = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clearance", meta = (ClampMin = "0.0", Units = "cm"))
	float MinimumDoorDistance = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clearance", meta = (ClampMin = "0.0", Units = "cm"))
	float MinimumWindowDistance = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Prop")
	TArray<FInteriorPCGAssetVariant> Variants;
};

UCLASS(BlueprintType)
class INTERIORPCGRUNTIME_API UInteriorPCGPropSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Props")
	TArray<FInteriorPCGPropDefinition> Props;

	const FInteriorPCGPropDefinition* FindProp(EInteriorPCGPropType PropType) const;
};

USTRUCT(BlueprintType)
struct INTERIORPCGRUNTIME_API FInteriorPCGFunctionalSetMember
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Member")
	EInteriorPCGPropType PropType = EInteriorPCGPropType::None;

	/** Relative to the set anchor. Translation is in centimeters. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Member")
	FTransform RelativeTransform;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Member")
	EInteriorPCGAnchorType AnchorOverride = EInteriorPCGAnchorType::Free;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Member")
	EInteriorPCGLookAtMode LookAtOverride = EInteriorPCGLookAtMode::KeepAssetForward;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Member")
	bool bRequired = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Member", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SpawnChance = 1.0f;
};

USTRUCT(BlueprintType)
struct INTERIORPCGRUNTIME_API FInteriorPCGFunctionalSetDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Set")
	FName SetID = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Set")
	TArray<EInteriorPCGRoomType> AllowedRoomTypes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Set", meta = (ClampMin = "0.0"))
	float Weight = 1.0f;

	/** Minimum area in square meters. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Set", meta = (ClampMin = "0.0"))
	float MinimumAreaSquareMeters = 0.0f;

	/** Zero disables the maximum. Value is in square meters. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Set", meta = (ClampMin = "0.0"))
	float MaximumAreaSquareMeters = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Set")
	TArray<FInteriorPCGFunctionalSetMember> Members;
};

UCLASS(BlueprintType)
class INTERIORPCGRUNTIME_API UInteriorPCGInteriorRuleSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sets")
	TArray<FInteriorPCGFunctionalSetDefinition> FunctionalSets;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clearance", meta = (ClampMin = "0.0", Units = "cm"))
	float MainPathWidth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clearance", meta = (ClampMin = "0.0", Units = "cm"))
	float DoorApproachDepth = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clearance", meta = (ClampMin = "0.0", Units = "cm"))
	float WindowApproachDepth = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement", meta = (ClampMin = "1", ClampMax = "128"))
	int32 PlacementAttempts = 12;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DetailSpawnChance = 0.35f;
};

/** Swapping only module/prop sets preserves every generation rule. */
UCLASS(BlueprintType)
class INTERIORPCGRUNTIME_API UInteriorPCGGenerationProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	TObjectPtr<UInteriorPCGBuildingRuleSet> BuildingRules = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	TObjectPtr<UInteriorPCGInteriorRuleSet> InteriorRules = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assets")
	TObjectPtr<UInteriorPCGBuildingModuleSet> BuildingModules = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assets")
	TObjectPtr<UInteriorPCGPropSet> InteriorProps = nullptr;
};
