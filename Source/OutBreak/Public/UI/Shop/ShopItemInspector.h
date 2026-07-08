// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShopItemInspector.generated.h"

class UTextBlock;
class UBorder;
class UVerticalBox;
class UKeyBindableBtn;

/**
 * 
 */
UCLASS()
class OUTBREAK_API UShopItemInspector : public UUserWidget
{
	GENERATED_BODY()
	
public:
	/* 아이템 이름 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_InspectorTitle;
	
	/* 이 아이템에 대한 인벤토리 보유 개수 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_InspectorQty;

	/* 아이템 메타 정보 ex) Advanced  •  Consumable */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_InspectorMeta;
	
	/* 아이템 이미지 (현재는 Border)*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UBorder> IMG_InspectorPreview_Placeholder;
	
	/* 아이템 설명 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_InspectorDescription;
	
	/* 아이템 스텟 정보 리스트 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UVerticalBox> VBX_ItemStatList;
	
	/* 아이템 가격 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_InspectorPriceValue;
	
	/* 배치된 버튼 0 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UKeyBindableBtn> BTN_0;
	
	/* 배치된 버튼 1 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UKeyBindableBtn> BTN_1;
	
	virtual void NativeConstruct() override;
};
