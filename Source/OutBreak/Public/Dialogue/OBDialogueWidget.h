// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Dialogue/OBDialogueTypes.h"
#include "OBDialogueWidget.generated.h"

class UTextBlock;
class UButton;
class UDataTable;

// NPC가 구독하는 액션 이벤트(이벤트 디스패처). Close/None/이동은 위젯 내부 처리.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOBOnDialogueAction, EOBDialogueAction, Action);

UCLASS()
class OUTBREAK_API UOBDialogueWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	// NPC가 상호작용 시 호출: 테이블 지정 + 시작 노드 표시.
	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void StartDialogue(UDataTable* InTable, FName StartNode);
	
	// NPC가 액션 처리 후 특정 노드로 분기시킬 때 호출.
	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void GoToNode(FName RowName) { ShowNode(RowName); }

	// NPC가 바인딩하는 액션 이벤트.
	UPROPERTY(BlueprintAssignable, Category = "Dialogue")
	FOBOnDialogueAction OnDialogueAction;

protected:
	virtual void NativeConstruct() override;

	// 노드 조회 → 텍스트/버튼 갱신.
	void ShowNode(FName RowName);

	// 4개 고정 버튼의 클릭 핸들러(인덱스별).
	UFUNCTION() 
	void OnOption0();
	UFUNCTION() 
	void OnOption1();
	UFUNCTION() 
	void OnOption2();
	UFUNCTION() 
	void OnOption3();
	UFUNCTION() 
	void OnCloseClicked();

	void HandleOption(int32 Index);

	//~ BindWidget: BP에서 같은 이름의 위젯을 배치하면 자동 연결.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Name;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Body;

	// 각 버튼
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_0;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_1;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_2;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_3;
	
	// 항상 표시되는 닫기 버튼(대사 노드의 옵션과 무관).
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Close;

	// 각 버튼 안의 라벨 텍스트(옵션명 표시용).
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Btn_0_Label;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Btn_1_Label;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Btn_2_Label;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Btn_3_Label;

private:
	UPROPERTY()
	TObjectPtr<UDataTable> Table;

	// 현재 노드의 선택지(버튼 인덱스 → 옵션).
	TArray<FOBDialogueOption> CurrentOptions;

	void SetButton(int32 Index, UButton* Btn, UTextBlock* Label);
};
