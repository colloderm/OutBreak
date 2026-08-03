#pragma once

#include "CoreMinimal.h"
#include "PCGVolume.h"
#include "InteriorPCGTypes.h"
#include "InteriorPCGVolume.generated.h"

class AActor;
class UInteriorPCGItemComponent;
class UInteriorPCGPreset;

UCLASS(BlueprintType)
class INTERIORPCGRUNTIME_API AInteriorPCGVolume : public APCGVolume
{
	GENERATED_BODY()

public:
	AInteriorPCGVolume(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior PCG|Generation")
	int32 Seed = 1337;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior PCG|Generation", meta = (TitleProperty = "Label"))
	TArray<FInteriorPCGAssetEntry> AssetEntries;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior PCG|Generation", meta = (ClampMin = "0"))
	int32 WeightedSelectionCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior PCG|Generation")
	TObjectPtr<UInteriorPCGPreset> SelectedPreset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior PCG|Generation", meta = (DisplayName = "Selected Presets (Batch)"))
	TArray<TObjectPtr<UInteriorPCGPreset>> SelectedPresets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior PCG|Generation")
	EInteriorPCGGraphGenerationMode GraphGenerationMode = EInteriorPCGGraphGenerationMode::RandomEntries;

	UPROPERTY(EditInstanceOnly, Transient, Category = "Interior PCG|Editing", meta = (DisplayName = "Props To Register"))
	TArray<TObjectPtr<AActor>> PropsToRegister;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior PCG|Collision", meta = (ClampMin = "1", ClampMax = "1000"))
	int32 MaxPlacementAttemptsPerItem = 40;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior PCG|Collision")
	TEnumAsByte<ECollisionChannel> FloorTraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior PCG|Collision")
	TEnumAsByte<ECollisionChannel> PlacementCollisionChannel = ECC_WorldDynamic;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior PCG|Collision", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinimumFloorNormalZ = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior PCG|Collision", meta = (ClampMin = "0.0"))
	float FloorTracePadding = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior PCG|Collision", meta = (ClampMin = "0.0"))
	float CollisionFloorClearance = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior PCG|Collision")
	bool bCheckWorldCollision = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior PCG|Collision")
	bool bCheckPresetCollision = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior PCG|Multi Floor")
	bool bGenerateOnAllDetectedFloors = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior PCG|Multi Floor", meta = (ClampMin = "1", ClampMax = "16", EditCondition = "bGenerateOnAllDetectedFloors"))
	int32 FloorDetectionSamplesPerAxis = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior PCG|Multi Floor", meta = (ClampMin = "1.0", EditCondition = "bGenerateOnAllDetectedFloors"))
	float FloorLayerHeightTolerance = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior PCG|Multi Floor", meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bGenerateOnAllDetectedFloors"))
	float MinimumFloorSampleCoverage = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior PCG|Multi Floor", meta = (ClampMin = "1", ClampMax = "32", EditCondition = "bGenerateOnAllDetectedFloors"))
	int32 MaximumDetectedFloorCount = 8;

	UPROPERTY(VisibleInstanceOnly, Transient, BlueprintReadOnly, Category = "Interior PCG|Multi Floor")
	TArray<float> LastDetectedFloorWorldHeights;

	UFUNCTION(BlueprintCallable, Category = "Interior PCG")
	int32 GenerateRandomInterior();

	UFUNCTION(BlueprintCallable, Category = "Interior PCG")
	int32 GenerateFromSelectedPreset();

	UFUNCTION(BlueprintCallable, Category = "Interior PCG")
	int32 GenerateFromSelectedPresets();

	UFUNCTION(BlueprintCallable, Category = "Interior PCG")
	int32 GenerateFromPreset(const UInteriorPCGPreset* Preset);

	UFUNCTION(BlueprintCallable, Category = "Interior PCG")
	int32 ClearGeneratedActors();

	UFUNCTION(BlueprintCallable, Category = "Interior PCG|Multi Floor")
	int32 ScanFloorLayers();

	UFUNCTION(BlueprintCallable, Category = "Interior PCG")
	bool RegisterActor(AActor* Actor);

	UFUNCTION(BlueprintCallable, Category = "Interior PCG")
	int32 CaptureCurrentPlacement(UInteriorPCGPreset* Preset);

	virtual void ExecuteGraphGeneration();
	void GetRegisteredActors(TArray<AActor*>& OutActors) const;
	void EnsureEntryIds();

#if WITH_EDITOR
	virtual void PostActorCreated() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	virtual void PostLoad() override;

private:
	UPROPERTY()
	TArray<TSoftObjectPtr<AActor>> RegisteredActors;

	bool TryFindRandomPlacement(const FInteriorPCGAssetEntry& Entry, FRandomStream& Stream, const FBox& LocalBounds, const TArray<FBox>& AcceptedBounds, FTransform& OutTransform, AActor*& OutFloorActor, bool bUseTargetFloor = false, float TargetFloorWorldZ = 0.0f) const;
	int32 GenerateFromPresetList(const TArray<const UInteriorPCGPreset*>& Presets);
	bool TraceAllFloorSurfacesAtWorldXY(const FVector2D& WorldXY, TArray<FHitResult>& OutHits, const AActor* AdditionalIgnoredActor = nullptr) const;
	bool DetectFloorLayers(TArray<float>& OutFloorWorldHeights) const;
	void TryAssignDefaultGraph();

protected:
	bool GetVolumeLocalBounds(FBox& OutLocalBounds) const;
	bool TraceFloorAtWorldXY(const FVector2D& WorldXY, FHitResult& OutHit, const AActor* AdditionalIgnoredActor = nullptr) const;
	bool TraceFloorAtWorldXYForLayer(const FVector2D& WorldXY, float TargetFloorWorldZ, FHitResult& OutHit, const AActor* AdditionalIgnoredActor = nullptr) const;
	bool IsPlacementClear(const FVector& ActorLocation, const FQuat& ActorRotation, const FVector& CollisionHalfExtent, const TArray<FBox>& AcceptedBounds, const AActor* FloorActor) const;
	AActor* SpawnConfiguredActor(EInteriorPCGAssetKind AssetKind, const TSoftObjectPtr<UStaticMesh>& StaticMesh, const TSoftClassPtr<AActor>& ActorClass, const FTransform& Transform, const FGuid& StableId, const FGuid& SourceEntryId, const FVector& CollisionHalfExtent, float FloorHeightOffset, bool bUserAdded, EInteriorPCGItemRole ItemRole = EInteriorPCGItemRole::FurnitureOrProp, int32 FloorIndex = INDEX_NONE);
	UInteriorPCGItemComponent* FindItemComponent(const AActor* Actor) const;
	static FBox BuildPlacementBounds(const FVector& ActorLocation, const FQuat& ActorRotation, const FVector& CollisionHalfExtent);
	static int32 MakeFloorSeed(int32 BaseSeed, int32 FloorIndex);
};
