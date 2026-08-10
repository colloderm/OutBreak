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
	AOBHelicopterExclusionVolume(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Helicopter")
	bool bBlockInsertion = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Helicopter")
	bool bBlockExtraction = true;

protected:
	virtual void PostInitializeComponents() override;

private:
	/** Keeps the brush usable by EncompassesPoint without blocking gameplay traces. */
	void ConfigureLogicalVolumeCollision();
};
