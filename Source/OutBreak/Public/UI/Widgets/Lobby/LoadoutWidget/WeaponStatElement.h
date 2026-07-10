// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WeaponStatElement.generated.h"


class UTextBlock;
class UProgressBar;

/**
 * 
 */
UCLASS()
class OUTBREAK_API UWeaponStatElement : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta =(BindWidget))
	TObjectPtr<UTextBlock> TXT_StatLabel;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta =(BindWidget))
	TObjectPtr<UProgressBar> PB_Percentage;
	
	void SetStat(FText& inName, float inPercent);
	void SetStatName(FText& inName);
	void SetPercent(float inPercent);
protected:
	virtual void NativeConstruct() override;
};
