// Fill out your copyright notice in the Description page of Project Settings.


#include "Item/Loot/OBLootContainer.h"

#include "Components/StaticMeshComponent.h"
#include "Game/GameState/OBExpeditionGameState.h"
#include "Inventory/Components/PlayerInventoryComponent.h"
#include "Item/Data/OBItemDefinition.h"
#include "Item/Loot/OBLootTable.h"
#include "Item/OBItemRegistry.h"
#include "Net/UnrealNetwork.h"
#include "Player/Controller/OBPlayerController.h"
#include "UI/Loot/OBLootWindow.h"
#include "TimerManager.h"

namespace
{
	void AppendStackAsInstances(
		const FOBItemStack& Stack,
		TArray<FInventoryData>& OutItems)
	{
		const FOBItemDefinitionRow* ItemRow =
			UOBItemRegistry::FindItem(Stack.ItemTag);
		if (!ItemRow || Stack.Count <= 0)
		{
			return;
		}

		const bool bEquippable =
			ItemRow->Category == EOBItemCategory::Weapon ||
			ItemRow->Category == EOBItemCategory::Equipment;
		const int32 MaxStack = bEquippable
			? 1
			: FMath::Max(1, ItemRow->MaxStack);
		int32 Remaining = Stack.Count;
		while (Remaining > 0)
		{
			FInventoryData& Item = OutItems.AddDefaulted_GetRef();
			Item.ItemTag = Stack.ItemTag;
			Item.ItemStack = FMath::Min(MaxStack, Remaining);
			Item.InstanceId = FGuid::NewGuid();
			Remaining -= Item.ItemStack;
		}
	}

	TArray<FInventoryData> ConvertStacksToInstances(
		const TArray<FOBItemStack>& Stacks)
	{
		TArray<FInventoryData> Instances;
		for (const FOBItemStack& Stack : Stacks)
		{
			AppendStackAsInstances(Stack, Instances);
		}
		return Instances;
	}
}

AOBLootContainer::AOBLootContainer()
{
	PrimaryActorTick.bCanEverTick = false;
	
	bReplicates = true;
	SetReplicateMovement(false); // 제자리에 있는 액터라 이동 복제는 낭비다.
	
	StaticMeshComp = CreateDefaultSubobject<UStaticMeshComponent>("StaticMeshComp");
	if (RootComponent)
	{
		StaticMeshComp->SetupAttachment(RootComponent);
	}
	else
	{
		SetRootComponent(StaticMeshComp);
	}
}

void AOBLootContainer::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(AOBLootContainer, Contents);
}

void AOBLootContainer::BeginPlay()
{
	Super::BeginPlay();
	
	if (HasAuthority())
	{
		if (bRollOnBeginPlay)
		{
			RollContents();
		}

		// RollContents를 안 타는 경로(SpawnWithContents)도 여기서 타이머를 건다.
		RestartDespawnTimer();
	}
}

void AOBLootContainer::RollContents()
{
	if (!HasAuthority()) return;
	
	const FOBLootTableRow* Row = LootTableRow.GetRow<FOBLootTableRow>(TEXT("OBLootContainer"));
	if (!Row)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Loot] %s: 드랍 테이블 행 미지정 → 빈 상자"), *GetName());
		return;
	}
	
	// 월드 파티션은 셀이 언로드→리로드되면 BeginPlay를 다시 부른다.
	// 매번 새로 굴리면 멀어졌다 돌아오기만 해도 재파밍이 되므로,
	// "세션 시드 + 이 액터의 고유 이름"으로 스트림을 만들어 결과를 고정한다.
	int32 SessionSeed = 0;
	if (const AOBExpeditionGameState* GS = GetWorld() ? GetWorld()->GetGameState<AOBExpeditionGameState>() : nullptr)
	{
		SessionSeed = GS->GetLootSeed();
	}
	
	// 0 = 아직 안 정해짐. 이대로 굴리면 모든 세션이 같은 결과가 되므로 크게 남긴다.
	if (SessionSeed == 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Loot] %s: 루팅 시드가 아직 0이다 → 매 세션 같은 내용물이 나온다. "
				 "GameMode가 BeginPlay 전에 SetLootSeed를 하는지 확인할 것."), *GetName());
	}
	
	FRandomStream Stream(static_cast<int32>(HashCombine(GetTypeHash(GetFName()), static_cast<uint32>(SessionSeed))));
	TArray<FOBItemStack> RolledStacks;
	Row->Roll(Stream, RolledStacks);
	Contents = ConvertStacksToInstances(RolledStacks);

	OnRep_Contents();   // OnRep은 서버에서 안 불린다. 호스트 화면을 위해 직접 호출.
}

void AOBLootContainer::SetContents(const TArray<FOBItemStack>& InItems)
{
	if (!HasAuthority()) return;
	
	Contents = ConvertStacksToInstances(InItems);
	OnRep_Contents();
}

void AOBLootContainer::SetItemInstances(
	const TArray<FInventoryData>& InItems)
{
	if (!HasAuthority()) return;

	Contents = InItems;
	Contents.RemoveAll(
		[](const FInventoryData& Item)
		{
			return !Item.ItemTag.IsValid() || Item.ItemStack <= 0;
		});
	for (FInventoryData& Item : Contents)
	{
		if (!Item.InstanceId.IsValid())
		{
			Item.InstanceId = FGuid::NewGuid();
		}
	}
	OnRep_Contents();
}

void AOBLootContainer::AddContent(const FGameplayTag& ItemTag, int32 Count)
{
	if (!HasAuthority()) return;

	AppendStackAsInstances(FOBItemStack(ItemTag, Count), Contents);
	OnRep_Contents();
}

void AOBLootContainer::OnRep_Contents()
{
	// 내용물이 바뀌면 남은 시간 기준도 바뀐다(있음 → 없음).
	// 클라에서 Destroy하면 안 되므로 서버에서만 다시 건다.
	if (HasAuthority())
	{
		RestartDespawnTimer();
	}
	
	OnContentsChanged.Broadcast();
}

void AOBLootContainer::Interact_Implementation(AOBPlayerController* PC)
{
	// 빈 상자를 열어봐야 할 일이 없다. 프롬프트도 이미 "비어 있음"이다.
	if (!PC || Contents.IsEmpty()) return;

	// 커서·입력모드·이동잠금은 컨트롤러가 처리한다. 우리는 대상만 물려준다.
	UUserWidget* Widget = PC->OpenInteractionWidget(InteractWidgetClass);
	if (UOBLootWindow* Window = Cast<UOBLootWindow>(Widget))
	{
		Window->BindToContainer(this);
	}

	Super::Interact_Implementation(PC);
}

AOBLootContainer* AOBLootContainer::SpawnWithContents(UWorld* World, TSubclassOf<AOBLootContainer> ContainerClass,
	const FTransform& SpawnTransform, const TArray<FOBItemStack>& Items)
{
	return SpawnWithItemInstances(
		World,
		ContainerClass,
		SpawnTransform,
		ConvertStacksToInstances(Items));
}

AOBLootContainer* AOBLootContainer::SpawnWithItemInstances(
	UWorld* World,
	TSubclassOf<AOBLootContainer> ContainerClass,
	const FTransform& SpawnTransform,
	const TArray<FInventoryData>& Items)
{
	if (!World || !ContainerClass) return nullptr;
	if (World->GetNetMode() == NM_Client) return nullptr;	// 스폰은 서버 권위
	if (Items.IsEmpty()) return nullptr;					// 빈 시체/ 빈 자루를 남기지 않음

	TArray<FInventoryData> ValidItems = Items;
	ValidItems.RemoveAll(
		[](const FInventoryData& Item)
		{
			return !Item.ItemTag.IsValid() || Item.ItemStack <= 0;
		});
	for (FInventoryData& Item : ValidItems)
	{
		if (!Item.InstanceId.IsValid())
		{
			Item.InstanceId = FGuid::NewGuid();
		}
	}
	if (ValidItems.IsEmpty()) return nullptr;
	
	AOBLootContainer* LootContainer = World->SpawnActorDeferred<AOBLootContainer>(
		ContainerClass, SpawnTransform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!LootContainer) return nullptr;
	
	// BeginPlay가 드랍테이블로 덮어쓰지 않도록 먼저 끈 뒤 내용물을 넣는다.
	LootContainer->bRollOnBeginPlay = false;
	LootContainer->Contents = MoveTemp(ValidItems);
	
	LootContainer->FinishSpawning(SpawnTransform);
	
	return LootContainer;
}

AOBLootContainer* AOBLootContainer::SpawnFromTable(UWorld* World, TSubclassOf<AOBLootContainer> ContainerClass,
	const FTransform& SpawnTransform, const FDataTableRowHandle& LootRow)
{
	const FOBLootTableRow* Row = LootRow.GetRow<FOBLootTableRow>(TEXT("OBLootContainer"));
	if (!Row) return nullptr;
	
	// 처치 드랍은 매번 달라야 하므로 시드를 고정하지 않는다(레벨 상자와 정반대)
	FRandomStream Stream(FMath::Rand());
	TArray<FOBItemStack> Items;
	Row->Roll(Stream, Items);
	
	return SpawnWithContents(World, ContainerClass, SpawnTransform, Items);
}

int32 AOBLootContainer::TryTakeItem(UPlayerInventoryComponent* Inventory, const FGameplayTag& ItemTag, int32 Count)
{
	if (!HasAuthority() || !Inventory || Count <= 0) return 0;

	FInventoryData* Found = Contents.FindByPredicate(
		[&ItemTag](const FInventoryData& Item)
		{
			return Item.ItemTag == ItemTag;
		});
	if (!Found || Found->ItemStack <= 0) return 0;

	// 클라가 수량을 부풀려 보내도 상자에 있는 만큼이 상한이다.
	const int32 Requested = FMath::Min(Count, Found->ItemStack);

	FInventoryData RequestedInstance = *Found;
	RequestedInstance.ItemStack = Requested;
	const int32 Moved = Inventory->TryAddItemInstance(RequestedInstance);
	if (Moved <= 0) return 0;

	Found->ItemStack -= Moved;
	Found = nullptr; // 아래 RemoveAll이 배열을 재배치한다. 다시 쓰지 않는다.

	Contents.RemoveAll(
		[](const FInventoryData& Item)
		{
			return Item.ItemStack <= 0;
		});

	OnRep_Contents(); // OnRep은 서버에서 안 불린다. 호스트 화면을 위해 직접 호출.
	
	return Moved + (Moved == Requested && Count > Moved
		? TryTakeItem(Inventory, ItemTag, Count - Moved)
		: 0);
}

int32 AOBLootContainer::TryTakeAll(UPlayerInventoryComponent* Inventory)
{
	if (!HasAuthority() || !Inventory) return 0;

	// 복사본을 돈다. TryTakeItem이 Contents를 건드린다.
	const TArray<FInventoryData> Snapshot = Contents;

	int32 Total = 0;
	for (const FInventoryData& Item : Snapshot)
	{
		Total += TryTakeItem(Inventory, Item.ItemTag, Item.ItemStack);
	}
	
	return Total;
}

FText AOBLootContainer::GetInteractPromptText_Implementation() const
{
	// 다 턴 상자를 또 열게 만들지 않는다.
	if (Contents.IsEmpty())
	{
		return NSLOCTEXT("OBLoot", "EmptyContainer", "비어 있음");
	}
	
	return Super::GetInteractPromptText_Implementation();
}

void AOBLootContainer::RestartDespawnTimer()
{
	if (!HasAuthority()) return;

	const float Delay = Contents.IsEmpty() ? DespawnDelayWhenEmpty : DespawnDelayWithItems;

	// 0 이하 = 영구 존치. 레벨 배치 상자가 여기에 해당한다.
	if (Delay <= 0.f)
	{
		GetWorldTimerManager().ClearTimer(DespawnTimer);
		return;
	}

	GetWorldTimerManager().SetTimer(DespawnTimer, this, &AOBLootContainer::HandleDespawn, Delay, false);
}

void AOBLootContainer::HandleDespawn()
{
	if (!HasAuthority()) return;

	// 컨트롤러의 대상 목록은 약참조라 다음 갱신(0.15초)에 저절로 정리된다.
	// 열려 있던 루팅 창도 컨테이너가 null이 되면 스스로 닫힌다.
	Destroy();
}
