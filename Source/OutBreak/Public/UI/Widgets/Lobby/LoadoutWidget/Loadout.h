// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/OBInteractionWidget.h"
#include "Weapon/Data/OBWeaponTypes.h"
#include "Loadout.generated.h"

class UOBWeaponCatalog;
class ULoadoutSelectionList;
class ULoadoutSelectionView;
class UOBLoadoutSubsystem;
class AOBWeaponBase;
class UTexture2D;

UCLASS()
class OUTBREAK_API ULoadout : public UOBInteractionWidget
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta =(BindWidget))
	TObjectPtr<ULoadoutSelectionList> LoadoutSelectionList;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta =(BindWidget))
	TObjectPtr<ULoadoutSelectionView> LoadoutSelectionView;
	
	// 스탯 바 정규화 최대값(밸런스에 맞춰 조정).
	UPROPERTY(EditAnywhere, Category = "Loadout") 
	float MaxDamage = 100.f;
	
	UPROPERTY(EditAnywhere, Category = "Loadout") 
	float MaxRPM = 1000.f;
	
	UPROPERTY(EditAnywhere, Category = "Loadout") 
	float MaxRecoil = 7.f;
	
	// 정확도 바가 50%가 되는 스프레드 값(감쇠 곡선 기준점). 낮출수록 스프레드 차이에 민감.
	UPROPERTY(EditAnywhere, Category = "Loadout") 
	float MaxSpread = 5.f;
	
	// 기동성 바 하한. 이 배율이 빈 바, 1.0이 꽉 찬 바.
	UPROPERTY(EditAnywhere, Category = "Loadout") 
	float MinMobilityMultiplier = 0.6f;
	
	// 스타터 지급 판정용. WBP_Loadout Class Defaults에 DA_WeaponCatalog 지정.
	UPROPERTY(EditAnywhere, Category = "Loadout")
	TObjectPtr<UOBWeaponCatalog> WeaponCatalog;
	
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 서브시스템 변경 통지 → 다음 틱에 전체 재구성.
	void HandleLoadoutChanged();
	void RefreshAll();
	
	UFUNCTION()
	void HandleWeaponClicked(TSubclassOf<AOBWeaponBase> WeaponClass);

	void RebuildStash();
	void RebuildSlots();

	// 표시 정보의 단일 진입점. 상점과 같은 우선순위(ItemTable → WeaponData 폴백)를 쓴다.
	// 이걸 어기면 카드에는 WeaponData 이름, 하단 스탯창에는 ItemTable 이름이 동시에 뜬다.
	bool GetWeaponDisplay(TSubclassOf<AOBWeaponBase> WeaponClass,
		FText& OutName, FText& OutCategory, FText& OutDesc, UTexture2D*& OutIcon) const;
	void ShowStats(TSubclassOf<AOBWeaponBase> WeaponClass);
	
	UOBLoadoutSubsystem* GetLoadout() const;
	
	void ShowDefaultStats();
	
	void BindCardClicks();
	
	UFUNCTION()
	void HandleCardClicked(EOBWeaponSlot WeaponSlot);

private:
	// 한 프레임에 여러 번 통지가 와도 재구성은 1회.
	bool bRefreshPending = false;
};
