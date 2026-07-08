// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "OBExpeditionMapData.generated.h"

class UTexture2D;

UCLASS(BlueprintType)
class OUTBREAK_API UOBExpeditionMapData : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:
	// 맵 선택 UI 표시명.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map")
	FText DisplayName;

	// 실제 로드할 레벨(ServerTravel 대상).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map")
	TSoftObjectPtr<UWorld> Level;
	
	// 지역 카드 썸네일.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map")
	TObjectPtr<UTexture2D> Thumbnail;

	// 난이도(★ 개수 표시용).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map", meta = (ClampMin = "0", ClampMax = "5"))
	int32 DifficultyStars = 1;

	// 카드 부제/설명(선택).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map")
	FText Description;

	// [핵심] 이 세션의 총 정원(PvPvE 전체). 기본 맵 12. 큰 맵은 이 값을 올림.
	// - GameSession->MaxPlayers로 들어가 PreLogin 단계에서 초과 접속을 거부.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Session", meta = (ClampMin = "1", ClampMax = "100"))
	int32 MaxSessionPlayers = 12;

	// 세션 길이(초). 15~30분 범위. 기본 1200(20분).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Session", meta = (ClampMin = "60"))
	int32 SessionLength = 1200;
	
	// [알파 IP-direct 테스트] 이 지역을 호스팅하는 데디 서버 주소(예: "127.0.0.1:7777").
	// 비우면 매칭 서브시스템의 기본 주소 사용. (실 매칭 붙으면 백엔드가 대체)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Session|Test")
	FString TestServerAddress;

	// (이후 확장) 개인 탈출구 마감시간, 필요아이템 태그 등도 여기로 모을 예정.
};
