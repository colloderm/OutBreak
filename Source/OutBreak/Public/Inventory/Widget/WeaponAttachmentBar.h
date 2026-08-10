#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Equipment/Data/OBEquipmentTypes.h"
#include "WeaponAttachmentBar.generated.h"

class UInventorySlot;
class UPanelWidget;
class UPlayerInventoryComponent;

/**
 * 무기 슬롯 하단의 부착물 표시 줄.
 *
 * 칸 개수는 DT_Weapons의 AttachmentSlots가 정한다(무기마다 다르다).
 * 표시 전용이다. 설치는 무기 슬롯에 드롭하는 기존 경로를 쓴다.
 */
UCLASS()
class OUTBREAK_API UWeaponAttachmentBar : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Inventory|Attachment")
	void SetInventorySource(UPlayerInventoryComponent* InInventory);

	UFUNCTION(BlueprintCallable, Category = "Inventory|Attachment")
	void RefreshAttachmentBar();

protected:
	virtual void NativePreConstruct() override;

	// 부착물 칸을 담을 패널. WBP에서 HorizontalBox 하나에 이 이름을 준다.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> AttachmentSlotBox;

	// 칸 하나에 쓸 위젯. WBP_InventorySlot을 그대로 지정한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Attachment")
	TSubclassOf<UInventorySlot> AttachmentSlotClass;

	// 어느 무기 슬롯을 따라갈지. 주무기 하단이면 PrimaryWeapon.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Attachment")
	EOBEquipmentSlot TargetWeaponSlot = EOBEquipmentSlot::PrimaryWeapon;
	
	// 칸 사이 좌우 여백. 0이면 칸들이 한 덩어리로 붙어 보인다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Attachment", Meta = (ClampMin = "0.0"))
	float SlotSpacing = 6.f;

private:
	UPROPERTY(Transient)
	TObjectPtr<UPlayerInventoryComponent> InventoryComponent;
	
};