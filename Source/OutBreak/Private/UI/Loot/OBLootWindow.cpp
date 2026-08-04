// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Loot/OBLootWindow.h"

#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Item/Data/OBItemDefinition.h"
#include "Item/Loot/OBLootContainer.h"
#include "Item/OBItemRegistry.h"
#include "Player/Controller/OBPlayerController.h"
#include "UI/Loot/OBLootEntryWidget.h"

void UOBLootWindow::NativeConstruct()
{
	Super::NativeConstruct();

	if (BTN_TakeAll)
	{
		BTN_TakeAll->OnClicked.AddDynamic(this, &UOBLootWindow::HandleTakeAllClicked);
	}
}

void UOBLootWindow::NativeDestruct()
{
	// 창이 닫혀도 컨테이너는 살아있다. 구독을 끊지 않으면 죽은 위젯이 호출된다.
	if (AOBLootContainer* Bound = Container.Get())
	{
		Bound->OnContentsChanged.RemoveAll(this);
	}
	Container = nullptr;

	Super::NativeDestruct();
}

void UOBLootWindow::BindToContainer(AOBLootContainer* InContainer)
{
	if (AOBLootContainer* Previous = Container.Get())
	{
		Previous->OnContentsChanged.RemoveAll(this);
	}

	Container = InContainer;

	if (InContainer)
	{
		// 서버가 처리한 결과가 복제로 돌아오면 목록이 저절로 갱신된다.
		InContainer->OnContentsChanged.AddUObject(this, &UOBLootWindow::Rebuild);
	}

	Rebuild();
}

void UOBLootWindow::Rebuild()
{
	if (!Box_Entries) return;

	AOBLootContainer* Bound = Container.Get();
	if (!Bound || Bound->IsEmptyContainer())
	{
		// 다 털었으면 빈 창을 남기지 않는다.
		Box_Entries->ClearChildren();
		RequestClose();
		return;
	}

	Box_Entries->ClearChildren();

	TArray<FOBItemStack> Items = Bound->GetContents();

	// 정렬 기준은 창고·상점과 같다: 카테고리 → SortOrder → 태그.
	Items.Sort(
		[](const FOBItemStack& A, const FOBItemStack& B)
		{
			const FOBItemDefinitionRow* RowA = UOBItemRegistry::FindItem(A.ItemTag);
			const FOBItemDefinitionRow* RowB = UOBItemRegistry::FindItem(B.ItemTag);

			// 표에 없는 건 맨 뒤로. 눈에 띄어야 CSV를 고친다.
			if (!RowA || !RowB) return RowA != nullptr;

			if (RowA->Category  != RowB->Category)  return RowA->Category  < RowB->Category;
			if (RowA->SortOrder != RowB->SortOrder) return RowA->SortOrder < RowB->SortOrder;
			return A.ItemTag.ToString() < B.ItemTag.ToString();
		});

	if (!EntryWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[Loot] %s: EntryWidgetClass 미지정 → 목록이 비어 보인다."), *GetName());
		return;
	}

	for (const FOBItemStack& Stack : Items)
	{
		if (Stack.IsEmpty()) continue;

		UOBLootEntryWidget* Entry = CreateWidget<UOBLootEntryWidget>(this, EntryWidgetClass);
		if (!Entry) continue;

		Entry->SetEntry(this, Stack.ItemTag, Stack.Count);
		Box_Entries->AddChild(Entry);
	}
}

void UOBLootWindow::RequestTake(const FGameplayTag& ItemTag, int32 Count)
{
	AOBLootContainer* Bound = Container.Get();
	AOBPlayerController* PC = GetOwningPlayer<AOBPlayerController>();
	if (!Bound || !PC || Count <= 0) return;

	// 로컬에서 미리 지우지 않는다. 가방이 꽉 차서 실패하면 되돌려야 하기 때문.
	PC->Server_TakeLoot(Bound, ItemTag, Count);
}

void UOBLootWindow::RequestTakeAll()
{
	AOBLootContainer* Bound = Container.Get();
	AOBPlayerController* PC = GetOwningPlayer<AOBPlayerController>();
	if (!Bound || !PC) return;

	PC->Server_TakeAllLoot(Bound);
}

void UOBLootWindow::HandleTakeAllClicked()
{
	RequestTakeAll();
}