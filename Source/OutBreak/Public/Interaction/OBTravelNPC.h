// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OBInteractableActor.h"
#include "OBTravelNPC.generated.h"

UCLASS()
class OUTBREAK_API AOBTravelNPC : public AOBInteractableActor
{
	GENERATED_BODY()

protected:
	virtual void Interact_Implementation(AOBPlayerController* PC) override;
	
};
