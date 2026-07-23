// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "OBDialogueTypes.generated.h"

// 선택지가 실행하는 행동. Close/None/이동은 위젯이 처리, 나머지는 NPC가 처리.
UENUM(BlueprintType)
enum class EOBDialogueAction : uint8
{
	None            UMETA(DisplayName = "None (대사만)"),
	GiveStarterKit  UMETA(DisplayName = "기본 장비 지급"),
	OpenShop        UMETA(DisplayName = "상점 열기"),
	StartExpedition UMETA(DisplayName = "탐험 시작"),
};

USTRUCT(BlueprintType)
struct FOBDialogueOption
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Label;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EOBDialogueAction Action = EOBDialogueAction::None;

	// 이 선택 후 이동할 노드. None이면 대화 종료.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName NextNode;
};

USTRUCT(BlueprintType)
struct FOBDialogueNode : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText SpeakerName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Meta = (MultiLine = true))
	FText Body;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FOBDialogueOption> Options;
};