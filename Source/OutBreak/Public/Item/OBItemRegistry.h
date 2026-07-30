// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "OBItemRegistry.generated.h"

class UOBItemDefinition;
struct FAssetData;

/**
왜 존재하는가?
 - 태그로 아이템 정의를 찾는 단 하나의 통로. 인벤토리(서버) / UI / 드랍테이블 / 상점이 전부 여기로 온다.
 - 목록을 손으로 등록하지 않는다. AssetManager가 지정 폴더를 스캔한 결과를 그대로 쓴다
   → 아이템을 늘리는 작업 = 폴더에 에셋 하나 만들고 저장.
무엇을 저장하는가?
 - 태그 → 정의 캐시. CDO에 담으므로 GC로 날아가지 않고, 머신당 한 번만 만들어진다.
멀티플레이 역할?
 - 서버/클라가 같은 에셋을 각자 읽는다. 태그만 복제하고 정의는 복제하지 않는다.
 */
UCLASS()
class OUTBREAK_API UOBItemRegistry : public UObject
{
	GENERATED_BODY()
	
public:
	// AssetManager에 등록할 Primary Asset Type 이름.
	// 프로젝트 세팅 > Asset Manager 의 Primary Asset Type 값과 글자까지 같아야 한다.
	static const FName ItemAssetType;

	// 태그로 정의 찾기. 미등록/스캔 실패면 nullptr.
	static const UOBItemDefinition* FindItem(const FGameplayTag& ItemTag);
	
	// 무기 클래스 → 아이템 태그(역방향). 창고/상점이 무기 클래스로 말할 때 태그로 바꿔준다.
	// 에셋 경로만 비교하므로 무기 BP를 로드하지 않는다.
	static FGameplayTag FindTagForWeaponClass(const UClass* WeaponClass);

private:
	void RebuildCache();

#if WITH_EDITOR
	void HandleAssetChanged(const FAssetData& AssetData);
#endif

	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UOBItemDefinition>> ItemCache;
	
	// 무기 정의만 담는 역인덱스. 값은 태그뿐이라 에셋을 붙들지 않는다.
	UPROPERTY(Transient)
	TMap<FSoftObjectPath, FGameplayTag> WeaponPathToTag;

	UPROPERTY(Transient)
	bool bCacheBuilt = false;

#if WITH_EDITOR
	bool bAssetHooksBound = false;
#endif
};
