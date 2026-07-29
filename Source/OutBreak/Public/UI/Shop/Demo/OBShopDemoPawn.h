// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "OBShopDemoPawn.generated.h"

class UCameraComponent;

UCLASS()
class OUTBREAK_API AOBShopDemoPawn : public APawn
{
	GENERATED_BODY()

public:
	AOBShopDemoPawn();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shop Demo")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shop Demo")
	TObjectPtr<UCameraComponent> CameraComponent;
};
