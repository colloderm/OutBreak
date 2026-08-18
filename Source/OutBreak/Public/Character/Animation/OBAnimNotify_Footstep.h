// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "OBAnimNotify_Footstep.generated.h"

/**
 * 발 접지 프레임에 배치하는 발소리 노티파이.
 *
 * 실제 재생은 액터의 UOBFootstepComponent가 한다. 노티파이는 어느 발인지만 들고 있어서,
 * 사운드·볼륨·감쇠를 애니메이션 수십 개에 중복 입력하지 않아도 된다.
 * 컴포넌트를 찾지 못하면 조용히 넘어간다(무기·표정 등 발이 없는 애니에도 안전).
 */
UCLASS(const, hidecategories = Object, collapsecategories, meta = (DisplayName = "OB Footstep"))
class OUTBREAK_API UOBAnimNotify_Footstep : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

	// 소리를 낼 발 소켓/본. 스켈레톤의 실제 이름과 맞출 것(GASP 기본은 foot_l / foot_r).
	UPROPERTY(EditAnywhere, Category = "Footstep")
	FName FootSocket = TEXT("foot_l");
};
