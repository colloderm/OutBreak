// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "FlowFieldSettings.generated.h"


class AHordeProxyHost;
class AHordeProxyActor;
class UAnimToTextureDataAsset;
/**
 * 
 */
UCLASS(Config = Game,
	DefaultConfig,
	meta = (DisplayName = "Flow Field"))
class OUTBREAK_API UFlowFieldSettings : public UDeveloperSettings
{
	GENERATED_BODY()
	
	
public:
	FORCEINLINE int32 GetMaxAgentCount() const
	{
		return MaxAgentCount;
	}
	
	FORCEINLINE float GetMaxVelocity() const
	{
		return MaxVelocity;
	}

	FORCEINLINE float GetSeparationRadius() const
	{
		return SeparationRadius;
	}

	FORCEINLINE float GetSeparationSteeringWeight() const
	{
		return SeparationSteeringWeight;
	}
	
	FORCEINLINE TSubclassOf<AHordeProxyHost> GetHordeProxyHostClass() const
	{
		return HordeProxyHostClass;
	}
	FORCEINLINE TSubclassOf<AHordeProxyActor> GetHordeProxyActorClass() const
	{
		return HordeProxyActorClass;
	}
	
	FORCEINLINE float GetFlowDirectionSmoothingAlpha() const
	{
		return FlowDirectionSmoothingAlpha;
	}
	
	FORCEINLINE TSoftObjectPtr<UAnimToTextureDataAsset> GetHordeVATDataAsset() const
	{
		return HordeVATDataAsset;
	}
	
	FORCEINLINE int GetAnimToTextureAutoPlayDataCount() const
	{
		return AnimToTextureAutoPlayDataCount;
	}
	
private:
	/** 한 프레임에 방향을 다시 질의할 수 있는 최대 Agent 수 */
	UPROPERTY(Config, EditAnywhere, Category = "Query Budget", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxFlowQueriesPerFrame = 64;
	
	/** 네트워크 상태 갱신의 최소 간격 */
	UPROPERTY(Config, EditAnywhere, Category = "Network Budget", meta = (ClampMin = "0.001", UIMin = "0.001", Units = "s"))
	float NetworkUpdateInterval = 0.1f;
	
	UPROPERTY(Config, EditAnywhere, Category = "Network Budget", meta = (ClampMin = "1", UIMin = "1"))
	int HordeUpdateBudget = 8;

	/** 뷰포트 밖 Agent에 적용하는 갱신 주기 배율 */
	UPROPERTY(Config, EditAnywhere, Category = "Significance", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float OffscreenUpdateIntervalScale = 4.0f;

	/** Flow Field 예산 시스템을 활성화할지 여부 */
	UPROPERTY(Config, EditAnywhere, Category = "General")
	bool bEnableBudgeting = true;
	
	UPROPERTY(Config, EditAnywhere, Category = "General")
	int32 MaxAgentCount = 500;
	
	UPROPERTY(Config, EditAnywhere, Category = "General|Movement")
	float MaxVelocity = 500.f;
	
	UPROPERTY(Config, EditAnywhere, Category = "General|Movement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FlowDirectionSmoothingAlpha = 0.35f;

	UPROPERTY(Config, EditAnywhere, Category = "Horde Movement|Separation", meta = (ClampMin = "0.0"))
	float SeparationRadius = 90.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Horde Movement|Separation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SeparationSteeringWeight = 0.35f;
	
	UPROPERTY(Config, EditAnywhere, Category = "HordeProxy|Class")
	TSubclassOf<AHordeProxyHost> HordeProxyHostClass;
	
	UPROPERTY(Config, EditAnywhere, Category = "HordeProxy|Class")
	TSubclassOf<AHordeProxyActor> HordeProxyActorClass;
	
	UPROPERTY(Config, EditAnywhere, Category = "HordeProxy|Animation")
	TSoftObjectPtr<UAnimToTextureDataAsset> HordeVATDataAsset;
	
	UPROPERTY(Config, EditAnywhere, Category = "HordeProxy|Animation")
	int AnimToTextureAutoPlayDataCount = 4;
	
};
