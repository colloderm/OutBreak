// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Shop/ShopWidgetTypes.h"
#include "KeyBindableBtn.generated.h"

class UButton;
class UTextBlock;

UCLASS()
class OUTBREAK_API UKeyBindableBtn : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Shop")
	void SetActionData(const FShopActionViewData& InData);

	UFUNCTION(BlueprintCallable, Category = "Shop")
	void SetActionEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Shop")
	void ClearActionData();

	UFUNCTION(BlueprintPure, Category = "Shop")
	FName GetActionId() const;

	UPROPERTY(BlueprintAssignable, Category = "Shop")
	FShopActionTriggeredSignature OnActionTriggered;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_BindedKey;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_Action;

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

protected:
	UFUNCTION()
	void HandleClicked();

	void BroadcastAction();
	bool CanTrigger() const;
	UButton* GetClickButton() const;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button;

	UPROPERTY(Transient)
	FShopActionViewData ActionData;

	bool bActionEnabled = false;
};
