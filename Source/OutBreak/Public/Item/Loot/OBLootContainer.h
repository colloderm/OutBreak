// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Interaction/OBInteractableActor.h"
#include "Item/Data/OBItemTypes.h"
#include "Engine/DataTable.h"
#include "OBLootContainer.generated.h"

class UPlayerInventoryComponent;
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
	
	// 서버 전용. 내용물이 있을 때만 스폰한다(빈 상자를 월드에 남기지 않는다).
	// 시체 / 바닥에 버린 아이템처럼 내용이 이미 정해진 경우.
	static AOBLootContainer* SpawnWithContents(UWorld* World, TSubclassOf<AOBLootContainer> ContainerClass,
		const FTransform& SpawnTransform, const TArray<FOBItemStack>& Items);

	// 서버 전용. 즉석에서 굴려 스폰한다(좀비 처치 등 매번 달라야 하는 드랍).
	static AOBLootContainer* SpawnFromTable(UWorld* World, TSubclassOf<AOBLootContainer> ContainerClass,
		const FTransform& SpawnTransform, const FDataTableRowHandle& LootRow);
	
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	
	// 서버: 드랍 테이블로 채운다. 같은 세션에서는 몇 번 호출해도 결과가 같다.
	void RollContents();
	
	// 서버: 내용물을 직접 지정/추가(좀비 드랍·시체·바닥 버리기가 쓴다).
	void SetContents(const TArray<FOBItemStack>& InItems);
	void AddContent(const FGameplayTag& ItemTag, int32 Count);
	
	// GetContents를 const 함수로 고친다(위젯이 const 포인터로도 읽어야 한다)
	const TArray<FOBItemStack>& GetContents() const { return Contents; }
	
	UFUNCTION(BlueprintPure, Category = "Loot")
	bool IsEmptyContainer() const { return Contents.IsEmpty(); }
	
	// 내용물이 바뀔 때. 루팅 UI가 구독
	FOBOnLootContentsChanged OnContentsChanged;
	
	// 서버: 가방에 들어가는 만큼만 옮기고 그만큼 상자에서 뺀다. 부분 이동을 허용한다.
	// 반환: 실제로 옮긴 개수(0이면 가방이 꽉 찼거나 그 아이템이 없다).
	int32 TryTakeItem(UPlayerInventoryComponent* Inventory, const FGameplayTag& ItemTag, int32 Count);

	// 서버: 들어있는 걸 전부 시도한다. 안 들어가는 건 상자에 남는다.
	int32 TryTakeAll(UPlayerInventoryComponent* Inventory);

	virtual FText GetInteractPromptText_Implementation() const override;
	
	virtual void Interact_Implementation(AOBPlayerController* PC) override;

protected:
	virtual void BeginPlay() override;
	
	UFUNCTION()
	void OnRep_Contents();
	
	// DT_LootTables의 행을 고른다. 에디터에서 드롭다운으로 선택된다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", Meta = (RowType = "/Script/OutBreak.OBLootTableRow"))
	FDataTableRowHandle LootTableRow;
	
	// 레벨 배치 상자는 켠다. 런타임 스폰(시체/바닥 픽업)은 끄고 SetContents로 직접 채운다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	bool bRollOnBeginPlay = true;
	
	// 내용물이 있을 때 남아 있는 시간. 0 이하면 사라지지 않는다(레벨 배치 상자).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Despawn", Meta = (ClampMin = "0.0", Units = "s"))
	float DespawnDelayWithItems = 0.f;

	// 비었을 때 남아 있는 시간. 마지막 아이템을 꺼내는 순간 이쪽으로 갈아끼워진다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Despawn", Meta = (ClampMin = "0.0", Units = "s"))
	float DespawnDelayWhenEmpty = 0.f;

	FTimerHandle DespawnTimer;
	
	UPROPERTY(ReplicatedUsing = OnRep_Contents)
	TArray<FOBItemStack> Contents;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Loot")
	TObjectPtr<UStaticMeshComponent> StaticMeshComp;

protected:
	// 서버 전용. 현재 내용물 상태에 맞는 시간으로 타이머를 다시 건다.
	void RestartDespawnTimer();
	void HandleDespawn();
	
};
