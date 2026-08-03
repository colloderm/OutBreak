#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "InteriorPCGTypes.h"
#include "InteriorPCGPreset.generated.h"

UCLASS(BlueprintType)
class INTERIORPCGRUNTIME_API UInteriorPCGPreset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset", meta = (TitleProperty = "StableId"))
	TArray<FInteriorPCGPresetItem> Items;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Preset")
	FVector SourceVolumeLocalSize = FVector::ZeroVector;
};
