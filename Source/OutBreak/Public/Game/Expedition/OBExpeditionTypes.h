// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OBExpeditionTypes.generated.h"

// 세션(Expedition) 진행 페이즈.
UENUM(BlueprintType)
enum class EOBExpeditionPhase : uint8
{
	Warmup     UMETA(DisplayName = "Warmup"),      // 로드/시작 대기
	InProgress UMETA(DisplayName = "In Progress"), // 진행(타이머 가동)
	Ended      UMETA(DisplayName = "Ended")        // 종료(결과/복귀)
};

// 플레이어 개인 결과 상태.
UENUM(BlueprintType)
enum class EOBPlayerExpeditionStatus : uint8
{
	Alive     UMETA(DisplayName = "Alive"),
	Downed    UMETA(DisplayName = "Downed"),      // 팀 다운(블리드아웃)
	Extracted UMETA(DisplayName = "Extracted"),   // 탈출 성공
	Dead      UMETA(DisplayName = "Dead")         // 사망/실패
};