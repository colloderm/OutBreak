#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OBExtractionSite.generated.h"

class AOBHelicopterRoute;
class USceneComponent;

/**
 * Optional authored replacement for a PersonalExtract TargetPoint. It provides
 * the transforms and spline references needed by the extraction helicopter.
 */
UCLASS(Blueprintable)
class OUTBREAK_API AOBExtractionSite : public AActor
{
	GENERATED_BODY()

public:
	AOBExtractionSite();

	UFUNCTION(BlueprintPure, Category = "Extraction Site")
	FTransform GetLandingTransform() const;

	UFUNCTION(BlueprintPure, Category = "Extraction Site")
	FTransform GetFlareTransform() const;

	UFUNCTION(BlueprintPure, Category = "Extraction Site")
	AOBHelicopterRoute* GetApproachRoute() const { return ApproachRoute; }

	UFUNCTION(BlueprintPure, Category = "Extraction Site")
	AOBHelicopterRoute* GetExitRoute() const { return ExitRoute; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> LandingAnchor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> FlareAnchor;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Extraction Site")
	TObjectPtr<AOBHelicopterRoute> ApproachRoute;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Extraction Site")
	TObjectPtr<AOBHelicopterRoute> ExitRoute;
};
