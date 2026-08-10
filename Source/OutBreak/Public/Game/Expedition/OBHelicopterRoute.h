#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Game/Expedition/OBHelicopterTypes.h"
#include "OBHelicopterRoute.generated.h"

class USceneComponent;
class USplineComponent;

/**
 * Authoring actor for helicopter flight paths. Shape FlightPath in the level or
 * in a Blueprint child. Runtime helicopter movement remains server-authoritative.
 */
UCLASS(Blueprintable)
class OUTBREAK_API AOBHelicopterRoute : public AActor
{
	GENERATED_BODY()

public:
	AOBHelicopterRoute();

	UFUNCTION(BlueprintPure, Category = "Helicopter Route")
	FTransform GetRouteTransform(float NormalizedDistance) const;

	UFUNCTION(BlueprintPure, Category = "Helicopter Route")
	float GetRouteLength() const;

	UFUNCTION(BlueprintPure, Category = "Helicopter Route")
	EOBHelicopterRoutePurpose GetPurpose() const { return Purpose; }

	UFUNCTION(BlueprintPure, Category = "Helicopter Route")
	uint8 GetTeamSlot() const { return TeamSlot; }

	UFUNCTION(BlueprintPure, Category = "Helicopter Route")
	bool IsLooping() const { return bLoop; }

	UFUNCTION(BlueprintPure, Category = "Helicopter Route")
	USplineComponent* GetFlightPath() const { return FlightPath; }

protected:
	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USplineComponent> FlightPath;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Helicopter Route")
	EOBHelicopterRoutePurpose Purpose = EOBHelicopterRoutePurpose::InsertionOrbit;

	/** Zero accepts any team. Non-zero reserves the route for this team slot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Helicopter Route")
	uint8 TeamSlot = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Helicopter Route")
	bool bLoop = false;
};
