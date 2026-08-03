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

	UFUNCTION(BlueprintCallable, Category = "Interior PCG")
	int32 GenerateRandomInterior();

	UFUNCTION(BlueprintCallable, Category = "Interior PCG")
	int32 GenerateFromSelectedPreset();

	UFUNCTION(BlueprintCallable, Category = "Interior PCG")
	int32 GenerateFromPreset(const UInteriorPCGPreset* Preset);

	UFUNCTION(BlueprintCallable, Category = "Interior PCG")
	int32 ClearGeneratedActors();

	UFUNCTION(BlueprintCallable, Category = "Interior PCG")
	bool RegisterActor(AActor* Actor);

	UFUNCTION(BlueprintCallable, Category = "Interior PCG")
	int32 CaptureCurrentPlacement(UInteriorPCGPreset* Preset);

	void ExecuteGraphGeneration();
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

	bool GetVolumeLocalBounds(FBox& OutLocalBounds) const;
	bool TraceFloorAtWorldXY(const FVector2D& WorldXY, FHitResult& OutHit, const AActor* AdditionalIgnoredActor = nullptr) const;
	bool IsPlacementClear(const FVector& ActorLocation, const FQuat& ActorRotation, const FVector& CollisionHalfExtent, const TArray<FBox>& AcceptedBounds, const AActor* FloorActor) const;
	bool TryFindRandomPlacement(const FInteriorPCGAssetEntry& Entry, FRandomStream& Stream, const FBox& LocalBounds, const TArray<FBox>& AcceptedBounds, FTransform& OutTransform, AActor*& OutFloorActor) const;
	AActor* SpawnConfiguredActor(EInteriorPCGAssetKind AssetKind, const TSoftObjectPtr<UStaticMesh>& StaticMesh, const TSoftClassPtr<AActor>& ActorClass, const FTransform& Transform, const FGuid& StableId, const FGuid& SourceEntryId, const FVector& CollisionHalfExtent, float FloorHeightOffset, bool bUserAdded);
	UInteriorPCGItemComponent* FindItemComponent(const AActor* Actor) const;
	void TryAssignDefaultGraph();
};
