#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InteriorPCGTypes.h"
#include "InteriorPCGItemComponent.generated.h"

class AInteriorPCGVolume;

UCLASS(ClassGroup = (InteriorPCG), meta = (BlueprintSpawnableComponent))
class INTERIORPCGRUNTIME_API UInteriorPCGItemComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInteriorPCGItemComponent();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interior PCG")
	FGuid StableId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interior PCG")
	FGuid SourceEntryId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior PCG")
	bool bIncludedInPreset = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior PCG")
	float FloorHeightOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior PCG", meta = (ClampMin = "1.0"))
	FVector CollisionHalfExtent = FVector(50.0, 50.0, 50.0);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interior PCG")
	EInteriorPCGAssetKind AssetKind = EInteriorPCGAssetKind::StaticMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interior PCG")
	EInteriorPCGItemRole Role = EInteriorPCGItemRole::FurnitureOrProp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interior PCG")
	int32 FloorIndex = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interior PCG")
	bool bUserAdded = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interior PCG")
	TSoftObjectPtr<AInteriorPCGVolume> Generator;
};
