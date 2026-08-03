#pragma once

#include "CoreMinimal.h"
#include "InteriorPCGTypes.generated.h"

class AActor;
class UStaticMesh;

UENUM(BlueprintType)
enum class EInteriorPCGAssetKind : uint8
{
	StaticMesh UMETA(DisplayName = "Static Mesh"),
	ActorClass UMETA(DisplayName = "Actor Class")
};

UENUM(BlueprintType)
enum class EInteriorPCGQuantityMode : uint8
{
	FixedCount UMETA(DisplayName = "Fixed Count"),
	WeightedPool UMETA(DisplayName = "Weighted Pool")
};

UENUM(BlueprintType)
enum class EInteriorPCGRotationMode : uint8
{
	Fixed UMETA(DisplayName = "Fixed Rotation"),
	RandomYaw UMETA(DisplayName = "Random Yaw"),
	SteppedRandomYaw UMETA(DisplayName = "Stepped Random Yaw")
};

UENUM(BlueprintType)
enum class EInteriorPCGGraphGenerationMode : uint8
{
	RandomEntries UMETA(DisplayName = "Random From Asset Entries"),
	SelectedPreset UMETA(DisplayName = "Selected Preset")
};

UENUM(BlueprintType)
enum class EInteriorPCGItemRole : uint8
{
	FurnitureOrProp UMETA(DisplayName = "Furniture Or Prop"),
	InteriorWall UMETA(DisplayName = "Interior Wall"),
	DoorWall UMETA(DisplayName = "Door Wall"),
	Stair UMETA(DisplayName = "Stair")
};

USTRUCT(BlueprintType)
struct INTERIORPCGRUNTIME_API FInteriorPCGAssetEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asset")
	FName Label = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Asset", AdvancedDisplay)
	FGuid EntryId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asset")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asset")
	EInteriorPCGAssetKind AssetKind = EInteriorPCGAssetKind::StaticMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asset", meta = (EditCondition = "AssetKind == EInteriorPCGAssetKind::StaticMesh", EditConditionHides))
	TSoftObjectPtr<UStaticMesh> StaticMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asset", meta = (EditCondition = "AssetKind == EInteriorPCGAssetKind::ActorClass", EditConditionHides, AllowAbstract = "false"))
	TSoftClassPtr<AActor> ActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quantity")
	EInteriorPCGQuantityMode QuantityMode = EInteriorPCGQuantityMode::FixedCount;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quantity", meta = (ClampMin = "0", EditCondition = "QuantityMode == EInteriorPCGQuantityMode::FixedCount", EditConditionHides))
	int32 Count = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quantity", meta = (ClampMin = "0.0", EditCondition = "QuantityMode == EInteriorPCGQuantityMode::WeightedPool", EditConditionHides))
	float SelectionWeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement", meta = (ClampMin = "1.0"))
	FVector CollisionHalfExtent = FVector(50.0, 50.0, 50.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	FVector PositionOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	FRotator RotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	EInteriorPCGRotationMode RotationMode = EInteriorPCGRotationMode::RandomYaw;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement", meta = (ClampMin = "1.0", ClampMax = "360.0", EditCondition = "RotationMode == EInteriorPCGRotationMode::SteppedRandomYaw", EditConditionHides))
	float YawStepDegrees = 90.0f;

	bool HasValidAssetReference() const;
};

USTRUCT(BlueprintType)
struct INTERIORPCGRUNTIME_API FInteriorPCGPresetItem
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Identity")
	FGuid StableId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Identity")
	FGuid SourceEntryId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	bool bEnabled = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Placement")
	EInteriorPCGItemRole Role = EInteriorPCGItemRole::FurnitureOrProp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Placement")
	int32 FloorIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asset")
	EInteriorPCGAssetKind AssetKind = EInteriorPCGAssetKind::StaticMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asset", meta = (EditCondition = "AssetKind == EInteriorPCGAssetKind::StaticMesh", EditConditionHides))
	TSoftObjectPtr<UStaticMesh> StaticMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asset", meta = (EditCondition = "AssetKind == EInteriorPCGAssetKind::ActorClass", EditConditionHides, AllowAbstract = "false"))
	TSoftClassPtr<AActor> ActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	FVector2D NormalizedPosition = FVector2D(0.5, 0.5);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	FRotator RelativeRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	FVector Scale = FVector::OneVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	float FloorHeightOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision", meta = (ClampMin = "1.0"))
	FVector CollisionHalfExtent = FVector(50.0, 50.0, 50.0);

	bool HasValidAssetReference() const;
};

struct INTERIORPCGRUNTIME_API FInteriorPCGPlacementMath
{
	static FGuid MakeStableGuid(FRandomStream& Stream);
	static float ResolveYaw(EInteriorPCGRotationMode Mode, float YawStepDegrees, FRandomStream& Stream);
	static FVector2D NormalizeLocalXY(const FVector& LocalPosition, const FBox& LocalBounds);
	static FVector DenormalizeLocalXY(const FVector2D& NormalizedPosition, const FBox& LocalBounds, double LocalZ = 0.0);
};
