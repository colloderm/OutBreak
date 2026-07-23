// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Loadout.generated.h"

class ULoadoutSelectionList;
class ULoadoutSelectionView;

/**
 * 
 */
UCLASS()
class OUTBREAK_API ULoadout : public UUserWidget
{
	GENERATED_BODY()
	
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta =(BindWidget))
	TObjectPtr<ULoadoutSelectionList> LoadoutSelectionList;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta =(BindWidget))
	TObjectPtr<ULoadoutSelectionView> LoadoutSelectionView;
	
	
protected:
	virtual void NativeConstruct() override;
};
