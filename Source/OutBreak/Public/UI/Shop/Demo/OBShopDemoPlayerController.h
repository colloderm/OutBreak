// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "UI/Shop/ShopDummyItemTypes.h"
#include "OBShopDemoPlayerController.generated.h"

class UShopWindow;

UCLASS()
class OUTBREAK_API AOBShopDemoPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AOBShopDemoPlayerController();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;

	UFUNCTION(BlueprintCallable, Category = "Shop Demo")
	void AddRandomItemToShop();

	UFUNCTION(BlueprintCallable, Category = "Shop Demo")
	void ResetDummyShop();

	UFUNCTION()
	void HandleShopActionRequested(FName ShopId, FName ItemId, FName ActionId, int32 Quantity);

	UFUNCTION()
	void HandleShopCloseRequested(FName ShopId);

	void HandleAddRandomItemPressed();
	bool EnsureShopWindow();
	void CreateShopWindow();
	void BindShopWindowDelegates();
	void UnbindShopWindowDelegates();
	void ApplyShopInputMode();
	FShopDummyShopData BuildInitialDummyShopData() const;
	FShopDummyItemData BuildRandomItemData();
	void NotifyDemoMessage(const FString& Message) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop Demo")
	TSubclassOf<UShopWindow> ShopWindowClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop Demo")
	FKey AddRandomItemKey = EKeys::R;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop Demo")
	bool bOpenShopOnBeginPlay = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop Demo")
	int32 InitialScrap = 500;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop Demo")
	int32 ShopViewportZOrder = 10;

	UPROPERTY(Transient)
	TObjectPtr<UShopWindow> ShopWindow;

	UPROPERTY(Transient)
	FShopDummyShopData ActiveShopData;

	int32 GeneratedItemSerial = 0;
};
