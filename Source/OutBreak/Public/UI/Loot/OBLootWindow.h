// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UI/Widgets/OBInteractionWidget.h"
#include "OBLootWindow.generated.h"

class AOBLootContainer;
class UButton;
class UPanelWidget;
class UTextBlock;
class UOBLootEntryWidget;

/**
 왜 존재하는가?
 - 컨테이너 내용물을 보여주고 가방으로 옮긴다.
 무엇을 저장하는가?
 - 컨테이너 약참조뿐. 목록은 매번 컨테이너의 복제된 Contents에서 다시 만든다.
 멀티플레이 역할?
 - 순수 표시. 실제 이동은 컨트롤러의 Server_TakeLoot이 서버에서 한다.
 */
UCLASS()
class OUTBREAK_API UOBLootWindow : public UOBInteractionWidget
{
	GENERATED_BODY()
	
public:
	// 컨테이너가 Interact에서 물려준다.
	void BindToContainer(AOBLootContainer* InContainer);

	// 엔트리가 부른다. 요청만 보내고 목록은 복제가 돌아오면 갱신된다.
	void RequestTake(const FGameplayTag& ItemTag, int32 Count);
	void RequestTakeInstance(const FGuid& InstanceId, int32 Count);

	UFUNCTION(BlueprintCallable, Category = "Loot")
	void RequestTakeAll();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void HandleTakeAllClicked();

	void Rebuild();

	// 목록 한 줄의 위젯. WBP_LootEntry를 지정한다.
	UPROPERTY(EditDefaultsOnly, Category = "Loot")
	TSubclassOf<UOBLootEntryWidget> EntryWidgetClass;

	// 줄이 쌓이는 세로 박스. 이름이 맞아야 컴파일 시점에 연결된다.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> Box_Entries;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> BTN_TakeAll;

private:
	TWeakObjectPtr<AOBLootContainer> Container;
	
};
