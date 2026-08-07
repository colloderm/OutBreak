#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Volume.h"
#include "OBHelicopterExclusionVolume.generated.h"

/** Place this volume over water, roofs, interiors, or other forbidden helicopter areas. */
UCLASS(Blueprintable)
class OUTBREAK_API AOBHelicopterExclusionVolume : public AVolume
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Helicopter")
	bool bBlockInsertion = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Helicopter")
	bool bBlockExtraction = true;
};
