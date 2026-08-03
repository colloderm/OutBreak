// Fill out your copyright notice in the Description page of Project Settings.


#include "Item/Loot/OBLootContainer.h"

#include "Components/StaticMeshComponent.h"
#include "Game/GameState/OBExpeditionGameState.h"
#include "Item/Loot/OBLootTable.h"
#include "Net/UnrealNetwork.h"


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
	
	if (HasAuthority() && bRollOnBeginPlay)
	{
		RollContents();
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
	Row->Roll(Stream, Contents);

	OnRep_Contents();   // OnRep은 서버에서 안 불린다. 호스트 화면을 위해 직접 호출.
}

void AOBLootContainer::SetContents(const TArray<FOBItemStack>& InItems)
{
	if (!HasAuthority()) return;
	
	Contents = InItems;
	OnRep_Contents();
}

void AOBLootContainer::AddContent(const FGameplayTag& ItemTag, int32 Count)
{
	if (!HasAuthority()) return;
	
	OBItemStacks::Add(Contents, ItemTag, Count);
	OnRep_Contents();
}

void AOBLootContainer::OnRep_Contents()
{
	OnContentsChanged.Broadcast();
}

void AOBLootContainer::Interact_Implementation(AOBPlayerController* PC)
{
	// 루팅 UI는 인벤토리 그리드 위젯이 나온 뒤에 붙인다(M4).
	// 지금은 복제가 제대로 되는지 확인할 수 있게 내용물만 찍는다.
#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Log, TEXT("[Loot] %s 내용물 %d종"), *GetName(), Contents.Num());
	for (const FOBItemStack& Stack : Contents)
	{
		UE_LOG(LogTemp, Log, TEXT("  - %s x%d"), *Stack.ItemTag.ToString(), Stack.Count);
	}
#endif

	Super::Interact_Implementation(PC);
}

AOBLootContainer* AOBLootContainer::SpawnWithContents(UWorld* World, TSubclassOf<AOBLootContainer> ContainerClass,
	const FTransform& SpawnTransform, const TArray<FOBItemStack>& Items)
{
	if (!World || !ContainerClass) return nullptr;
	if (World->GetNetMode() == NM_Client) return nullptr;	// 스폰은 서버 권위
	if (Items.IsEmpty()) return nullptr;					// 빈 시체/ 빈 자루를 남기지 않음
	
	AOBLootContainer* LootContainer = World->SpawnActorDeferred<AOBLootContainer>(
		ContainerClass, SpawnTransform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!LootContainer) return nullptr;
	
	// BeginPlay가 드랍테이블로 덮어쓰지 않도록 먼저 끈 뒤 내용물을 넣는다.
	LootContainer->bRollOnBeginPlay = false;
	LootContainer->Contents = Items;
	
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
