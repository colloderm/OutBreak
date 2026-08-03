#pragma once

#include "CoreMinimal.h"
#include "InteriorPCGVolume.h"
#include "InteriorPCGWallVolume.generated.h"

class AActor;

UENUM(BlueprintType)
enum class EInteriorPCGWallDirectionMode : uint8
{
	RandomPerPartition UMETA(DisplayName = "Random X Or Y"),
	VolumeLocalX UMETA(DisplayName = "Volume Local X"),
	VolumeLocalY UMETA(DisplayName = "Volume Local Y")
};

USTRUCT(BlueprintType)
struct INTERIORPCGRUNTIME_API FInteriorPCGWallClassEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall Class")
	FName Label = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wall Class", AdvancedDisplay)
	FGuid EntryId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall Class")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall Class")
	EInteriorPCGAssetKind AssetKind = EInteriorPCGAssetKind::ActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall Class", meta = (EditCondition = "AssetKind == EInteriorPCGAssetKind::StaticMesh", EditConditionHides))
	TSoftObjectPtr<UStaticMesh> StaticMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall Class", meta = (EditCondition = "AssetKind == EInteriorPCGAssetKind::ActorClass", EditConditionHides, AllowAbstract = "false"))
	TSoftClassPtr<AActor> ActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall Class", meta = (ClampMin = "0.0"))
	float SelectionWeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	FVector PositionOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	FRotator RotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	FVector Scale = FVector::OneVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision", meta = (ClampMin = "1.0"))
	FVector CollisionHalfExtent = FVector(49.0, 10.0, 150.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Connectivity")
	FVector LowerAccessPointOffset = FVector(100.0, 0.0, 0.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Connectivity")
	FVector UpperAccessPointOffset = FVector(-100.0, 0.0, 0.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Connectivity")
	FVector DoorAccessPointOffset = FVector::ZeroVector;

	bool HasValidAssetReference() const;
	bool HasValidClass() const;
};

/**
 * Interior generator with an optional child workflow for seeded partition walls.
 * Wall and door-wall classes are selected only from the arrays explicitly filled by the user.
 */
UCLASS(BlueprintType, meta = (DisplayName = "Interior PCG Wall Volume"))
class INTERIORPCGRUNTIME_API AInteriorPCGWallVolume : public AInteriorPCGVolume
{
	GENERATED_BODY()

public:
	AInteriorPCGWallVolume(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior PCG|Walls|Generation")
	int32 WallSeed = 7331;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior PCG|Walls|Generation", meta = (ClampMin = "0"))
	int32 PartitionWallCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior PCG|Walls|Generation")
	EInteriorPCGWallDirectionMode WallDirectionMode = EInteriorPCGWallDirectionMode::RandomPerPartition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior PCG|Walls|Classes", meta = (TitleProperty = "Label"))
	TArray<FInteriorPCGWallClassEntry> WallClasses;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior PCG|Walls|Classes", meta = (TitleProperty = "Label"))
	TArray<FInteriorPCGWallClassEntry> DoorWallClasses;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior PCG|Walls|Classes", meta = (TitleProperty = "Label"))
	TArray<FInteriorPCGWallClassEntry> StairClasses;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior PCG|Walls|Connectivity")
	bool bRequireConnectedDoorAndStairPaths = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior PCG|Walls|Connectivity", meta = (ClampMin = "1", EditCondition = "bRequireConnectedDoorAndStairPaths"))
	int32 MaxStairPlacementAttempts = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior PCG|Walls|Connectivity", meta = (ClampMin = "10.0", EditCondition = "bRequireConnectedDoorAndStairPaths"))
	float WalkwayHalfWidth = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior PCG|Walls|Connectivity", meta = (ClampMin = "10.0", EditCondition = "bRequireConnectedDoorAndStairPaths"))
	float WalkwayHalfHeight = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior PCG|Walls|Connectivity", meta = (ClampMin = "10.0", EditCondition = "bRequireConnectedDoorAndStairPaths"))
	float WalkwaySampleSpacing = 50.0f;

	UPROPERTY(VisibleInstanceOnly, Transient, BlueprintReadOnly, Category = "Interior PCG|Walls|Connectivity")
	TArray<FVector> LastConnectivityPathPoints;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior PCG|Walls|Door", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DoorChancePerPartition = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior PCG|Walls|Door", meta = (ClampMin = "0"))
	int32 DoorEndPaddingModules = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior PCG|Walls|Scan", meta = (ClampMin = "1.0"))
	float WallModuleLength = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior PCG|Walls|Scan", meta = (ClampMin = "0.0"))
	float WallScanHeight = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior PCG|Walls|Scan", meta = (ClampMin = "0.0"))
	float WallEndClearance = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior PCG|Walls|Scan", meta = (ClampMin = "0.0"))
	float PartitionMargin = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior PCG|Walls|Scan", meta = (ClampMin = "1"))
	int32 MaxWallPlacementAttempts = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior PCG|Walls|Scan")
	TEnumAsByte<ECollisionChannel> WallTraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior PCG|Walls|Scan", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaximumBoundaryWallNormalZ = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior PCG|Walls|PCG Graph")
	bool bGenerateWallsWithRandomGraphGeneration = true;

	UFUNCTION(BlueprintCallable, Category = "Interior PCG|Walls")
	int32 GenerateInteriorWalls();

	UFUNCTION(BlueprintCallable, Category = "Interior PCG|Walls")
	int32 GenerateWallsAndInterior();

	UFUNCTION(BlueprintCallable, Category = "Interior PCG|Walls")
	int32 ClearGeneratedWalls();

	virtual void ExecuteGraphGeneration() override;

	void EnsureWallEntryIds();

#if WITH_EDITOR
	virtual void PostActorCreated() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	virtual void PostLoad() override;

private:
	const FInteriorPCGWallClassEntry* SelectWallClass(const TArray<FInteriorPCGWallClassEntry>& Entries, FRandomStream& Stream) const;
	bool FindBoundarySpan(const FVector& ScanOrigin, const FVector& Direction, FVector& OutNegativeHit, FVector& OutPositiveHit) const;
	bool TryBuildConnectivityCorridor(const FVector& StartWorld, const FVector& EndWorld, float TargetFloorWorldZ, const TArray<FBox>& NavigationObstacleBounds, bool bTryLocalXFirst, TArray<FBox>& OutCorridorBounds, TArray<FVector>& OutPathPoints) const;
	bool IsConnectivitySampleClear(const FVector2D& WorldXY, float TargetFloorWorldZ, const TArray<FBox>& NavigationObstacleBounds, FBox& OutCorridorBound, FVector& OutPathPoint) const;
};
