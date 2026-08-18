// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "OBGameplayCueNotify_ImpactDecal.generated.h"

class UMaterialInterface;

/**
 * 탄흔 데칼. GameplayCue.Weapon.ImpactDecal 로 실행된다.
 *
 * 발사 능력이 서버에서 명중을 확정한 뒤 큐를 멀티캐스트하므로, 이 클래스는
 * 각 클라이언트에서 로컬로 데칼만 찍는다. 복제도 RPC도 필요 없다.
 *
 * 기존 GC_Weapon_Impact(VFX/SFX) BP와 태그를 분리한 이유는 태그 헤더 주석 참조.
 *
 * 사용법: 이 클래스를 상속한 BP를 만들고 GameplayCue Tag 와 Decal Material 을 채운다.
 * C++ CDO에 태그를 박지 않는 이유 — BP 자식과 둘 다 같은 태그를 주장하면
 * 큐 매니저가 중복으로 보고 하나만 살아남는다.
 */
UCLASS(Blueprintable)
class OUTBREAK_API UOBGameplayCueNotify_ImpactDecal : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	virtual bool OnExecute_Implementation(
		AActor* MyTarget,
		const FGameplayCueParameters& Parameters) const override;

protected:
	// 비어 있으면 아무것도 찍지 않고 경고를 남긴다.
	UPROPERTY(EditDefaultsOnly, Category = "Impact Decal")
	TObjectPtr<UMaterialInterface> DecalMaterial;

	// 랜덤 크기(cm). X=최소, Y=최대. 같은 구멍이 반복돼 보이는 걸 막는다.
	UPROPERTY(EditDefaultsOnly, Category = "Impact Decal")
	FVector2D DecalSizeRange = FVector2D(6.f, 10.f);

	// 표면을 파고드는 깊이(cm). 얇으면 굴곡면에서 잘리고, 두꺼우면 옆면까지 번진다.
	UPROPERTY(EditDefaultsOnly, Category = "Impact Decal", meta = (ClampMin = "1.0"))
	float DecalThickness = 8.f;

	// 생성 후 사라질 때까지(초). 교전이 길어져도 데칼이 무한히 쌓이지 않게 한다.
	UPROPERTY(EditDefaultsOnly, Category = "Impact Decal", meta = (ClampMin = "0.1"))
	float LifeSpan = 20.f;

	// 수명 끝에서 이 시간 동안 페이드아웃.
	UPROPERTY(EditDefaultsOnly, Category = "Impact Decal", meta = (ClampMin = "0.0"))
	float FadeOutDuration = 3.f;

	// 화면 점유율이 이보다 작아지면 그리지 않는다. 원거리 데칼 비용 차단.
	UPROPERTY(EditDefaultsOnly, Category = "Impact Decal", meta = (ClampMin = "0.0"))
	float FadeScreenSize = 0.01f;
};
