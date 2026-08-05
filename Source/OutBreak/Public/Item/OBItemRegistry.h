// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "Data/OBItemTypes.h"
#include "UObject/SoftObjectPath.h"
#include "OBItemRegistry.generated.h"

class UTexture2D;
class UDataTable;
struct FOBItemDefinitionRow;

/**
왜 존재하는가?
 - 태그로 아이템 스펙을 찾는 단 하나의 통로. 인벤토리/UI/드랍테이블/상점이 전부 여기로 온다.
 - 표(DataTable) 참조를 프로젝트 세팅 한 곳에만 두고, 조회는 전역 static으로 연다.
무엇을 저장하는가?
 - DT_Items / DT_LootTables 참조 + 태그→행 캐시.
멀티플레이 역할?
 - 모든 머신이 같은 표를 읽는다. 복제 불필요.
 */
UCLASS(Config = Game, DefaultConfig, Meta = (DisplayName = "OutBreak Items"))
class OUTBREAK_API UOBItemRegistry : public UDeveloperSettings
{
	GENERATED_BODY()
	
public:
	// 행 구조체 = FOBItemDefinitionRow. CSV에서 임포트한다.
	UPROPERTY(Config, EditDefaultsOnly, Category = "Data", Meta = (AllowedClasses = "/Script/Engine.DataTable"))
	TSoftObjectPtr<UDataTable> ItemTable;

	// 행 구조체 = FOBLootTableRow. JSON에서 임포트한다.
	UPROPERTY(Config, EditDefaultsOnly, Category = "Data", Meta = (AllowedClasses = "/Script/Engine.DataTable"))
	TSoftObjectPtr<UDataTable> LootTable;
	
public:
	// 태그로 스펙 찾기. 표에 없으면 nullptr.
	static const FOBItemDefinitionRow* FindItem(const FGameplayTag& ItemTag);

	// 무기 클래스 → 아이템 태그(역방향). 에셋 경로만 비교하므로 무기 BP를 로드하지 않는다.
	static FGameplayTag FindTagForWeaponClass(const UClass* WeaponClass);
	
	// 등록된 전체 아이템(상점 구매 목록 등). 순서는 보장하지 않는다.
	static void GetAllItems(TArray<const FOBItemDefinitionRow*>& OutItems);
	
	// 태그 → 화면에 그릴 이름/아이콘. 행의 DisplayName·Icon이 비면 무기 데이터에서 상속한다.
	// 표시 규칙을 여기 한 곳에만 둔다(상점·루팅·인벤토리가 같은 결과를 내야 한다).
	// 반환값: 표에 행이 있었는가. false여도 OutName에는 태그 문자열이 들어간다.
	static bool GetItemDisplay(const FGameplayTag& ItemTag, FText& OutName, UTexture2D*& OutIcon);
	
	// 창고 / 상점 / 루팅이 같은 순서로 보이도록. 카테고리 → SortOrder → 태그.
	static void SortStacks(TArray<FOBItemStack>& Stacks);

	// 드랍 테이블이 담긴 표. 컨테이너의 RowHandle이 이걸 가리켜야 한다.
	static UDataTable* GetLootTable();

	virtual FName GetCategoryName() const override { return TEXT("Game"); }
	
	// 표가 바뀌었을 때 다시 접기 위해 외부에서도 부를 수 있어야 한다.
	void EnsureCache();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	void RebuildCache();

	TMap<FGameplayTag, const FOBItemDefinitionRow*> ItemCache;
	TMap<FSoftObjectPath, FGameplayTag> WeaponPathToTag;

	bool bCacheBuilt = false;
};
