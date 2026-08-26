// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/Animation/OBAnimNotify_Footstep.h"

#include "Character/Components/OBFootstepComponent.h"
#include "Components/SkeletalMeshComponent.h"

void UOBAnimNotify_Footstep::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!IsValid(MeshComp))
	{
		return;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner)
	{
		return;
	}

	// 플레이어(AOBCharacterBase)와 좀비(AEnemyCharacter)는 공통 베이스가 없다.
	// 컴포넌트로 찾으면 캐스팅 분기 없이 양쪽에 같은 노티파이를 쓸 수 있다.
	if (UOBFootstepComponent* Footstep = Owner->FindComponentByClass<UOBFootstepComponent>())
	{
		Footstep->PlayFootstep(MeshComp, FootSocket);
	}
}

FString UOBAnimNotify_Footstep::GetNotifyName_Implementation() const
{
	// 타임라인에서 좌/우를 바로 구분할 수 있게 소켓 이름을 노출한다.
	return FString::Printf(TEXT("Footstep (%s)"), *FootSocket.ToString());
}
