#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Volume.h"
#include "OBHelicopterInsertionAreaVolume.generated.h"

/**
 * Authoritative allow-list volume for helicopter insertion targets.
 * At least one enabled volume must contain both the requested ground point and
 * the resolved landing point when landing-area enforcement is enabled.
 */
UCLASS(Blueprintable)
class OUTBREAK_API AOBHelicopterInsertionAreaVolume : public AVolume
{
	GENERATED_BODY()

public:
	AOBHelicopterInsertionAreaVolume(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Helicopter|Insertion")
	bool bAllowInsertion = true;

protected:
	virtual void PostInitializeComponents() override;

private:
	/** Keeps the brush usable by EncompassesPoint without blocking gameplay traces. */
	void ConfigureLogicalVolumeCollision();
};
