// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ShopWidgetTypes.generated.h"

/**
 * 상점 UI가 비어 있거나 갱신 중일 때 어떤 안내 상태를 표시할지 구분한다.
 * C++는 이 값을 통해 상태만 전달하고, 실제 문구와 배치는 WBP에서 조정한다.
 */
UENUM(BlueprintType)
enum class EShopWidgetEmptyState : uint8
{
	None,
	NoVendor,
	NoCategories,
	NoItems,
	NoSelection,
	Error
};

/**
 * 상점 위젯 계층에서 사용하는 공통 로그 카테고리다.
 * 잘못된 ListView Entry Data처럼 UI 계약이 깨진 경우 조용히 실패하지 않고 경고를 남긴다.
 */
DECLARE_LOG_CATEGORY_EXTERN(LogShopWidget, Log, All);
