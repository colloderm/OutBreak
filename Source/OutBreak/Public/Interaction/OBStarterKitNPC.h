// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OBDialogueNPC.h"
#include "OBStarterKitNPC.generated.h"

class AOBWeaponBase;

UCLASS()
class OUTBREAK_API AOBStarterKitNPC : public AOBDialogueNPC
{
	GENERATED_BODY()

protected:
	virtual void HandleAction(EOBDialogueAction Action) override;

	UPROPERTY(EditAnywhere, Category = "StarterKit")
	TSubclassOf<AOBWeaponBase> StarterPrimary;

	UPROPERTY(EditAnywhere, Category = "StarterKit")
	TSubclassOf<AOBWeaponBase> StarterSecondary;

	UPROPERTY(EditAnywhere, Category = "StarterKit")
	TSubclassOf<AOBWeaponBase> StarterMelee;

	// 결과 분기 노드.
	UPROPERTY(EditAnywhere, Category = "StarterKit")
	FName GaveNode = TEXT("AfterGive");

	UPROPERTY(EditAnywhere, Category = "StarterKit")
	FName AlreadyHaveNode = TEXT("AlreadyHave");
};
