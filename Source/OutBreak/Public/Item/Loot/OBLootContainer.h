// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Dialogs/Dialogs.h"
#include "Interaction/OBInteractableActor.h"
#include "Item/Data/OBItemTypes.h"
#include "OBLootContainer.generated.h"

class UOBLootTable;
class UStaticMeshComponent;

DECLARE_MULTICAST_DELEGATE(FOBOnLootContentsChanged);

/**
왜 존재하는가?
 - 레벨 배치 상자 / 좀비 드랍 / 플레이어 시체 / 바닥 픽업이 전부 같은 "아이템 담는 액터"다.
   하나로 두고 채우는 방식만 다르게 한다.
무엇을 저장하는가?
 - FOBItemStack 목록(복제). 실제 아이템 스펙은 태그로 레지스트리에서 찾는다.
멀티플레이 역할?
 - 채우는 건 서버 권위. 클라는 복제된 Contents만 읽는다.
 */

UCLASS()
class OUTBREAK_API AOBLootContainer : public AOBInteractableActor
{
	GENERATED_BODY()

public:
	AOBLootContainer();
	
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	
	// 서버: 드랍 테이블로 채운다. 같은 세션에서는 몇 번 호출해도 결과가 같다.
	void RollContents();
	
	// 서버: 내용물을 직접 지정/추가(좀비 드랍·시체·바닥 버리기가 쓴다).
	void SetContents(const TArray<FOBItemStack>& InItems);
	void AddContent(const FGameplayTag& ItemTag, int32 Count);
	
	const TArray<FOBItemStack>& GetContents() { return Contents; }
	
	UFUNCTION(BlueprintPure, Category = "Loot")
	bool IsEmptyContainer() const { return Contents.IsEmpty(); }
	
	// 내용물이 바뀔 때. 루팅 UI가 구독
	FOBOnLootContentsChanged OnContentsChanged;
	
	virtual void Interact_Implementation(AOBPlayerController* PC) override;

protected:
	virtual void BeginPlay() override;
	
	UFUNCTION()
	void OnRep_Contents();
	
	// 이 상자가 쓸 드랍 테이블. 비우면 빈 상자가 된다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	TObjectPtr<UOBLootTable> LootTable;
	
	// 레벨 배치 상자는 켠다. 런타임 스폰(시체/바닥 픽업)은 끄고 SetContets로 직접 채운다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	bool bRollOnBeginPlay = true;
	
	UPROPERTY(ReplicatedUsing = OnRep_Contents)
	TArray<FOBItemStack> Contents;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Loot")
	TObjectPtr<UStaticMeshComponent> StaticMeshComp;

};
