// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Cues/OBGameplayCueNotify_ImpactDecal.h"

#include "Components/DecalComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

bool UOBGameplayCueNotify_ImpactDecal::OnExecute_Implementation(
	AActor* MyTarget,
	const FGameplayCueParameters& Parameters) const
{
	// MyTarget은 큐를 실행한 ASC의 소유자(= 사격한 캐릭터)다. 월드를 얻는 용도로만 쓴다.
	const UWorld* World = IsValid(MyTarget) ? MyTarget->GetWorld() : nullptr;
	if (!World)
	{
		return false;
	}

	// 데디 서버는 그릴 화면이 없다.
	if (World->GetNetMode() == NM_DedicatedServer)
	{
		return false;
	}

	// 맞은 표면을 고른다. PhysicalMaterial이 없으면 DetermineSurfaceType이 Default를 준다.
	const EPhysicalSurface Surface =
		UPhysicalMaterial::DetermineSurfaceType(Parameters.PhysicalMaterial.Get());

	// 표면 전용 → 폴백 순. 맵에 키는 있는데 값이 비어 있는 경우도 폴백으로 흘린다.
	UMaterialInterface* Material = DecalMaterial;
	if (const TObjectPtr<UMaterialInterface>* Found = SurfaceDecals.Find(Surface))
	{
		if (*Found)
		{
			Material = *Found;
		}
	}

	if (!Material)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[ImpactDecal] %s 에 Decal Material이 비어 있다. "
				 "이 클래스를 상속한 큐 BP에서 지정할 것."), *GetNameSafe(GetClass()));
		return false;
	}

	// 법선이 0이면 회전을 만들 수 없다. 바닥으로 가정하는 편이 안 찍는 것보다 낫다.
	const FVector SurfaceNormal = Parameters.Normal.IsNearlyZero()
		? FVector::UpVector
		: Parameters.Normal.GetSafeNormal();

	// 데칼은 로컬 -X 방향으로 투영된다. 법선의 반대를 바라봐야 표면에 붙는다.
	FRotator DecalRotation = (-SurfaceNormal).Rotation();

	// 롤을 흔들지 않으면 같은 벽의 탄흔이 전부 같은 각도라 패턴이 눈에 띈다.
	DecalRotation.Roll = FMath::FRandRange(0.f, 360.f);

	const float MinSize = FMath::Min(DecalSizeRange.X, DecalSizeRange.Y);
	const float MaxSize = FMath::Max(DecalSizeRange.X, DecalSizeRange.Y);
	const float Size = FMath::FRandRange(MinSize, MaxSize);

	// LifeSpan을 여기서 주면 페이드 도중에 잘려 사라진다. 수명은 아래 SetFadeOut이 맡는다.
	UDecalComponent* Decal = UGameplayStatics::SpawnDecalAtLocation(
		MyTarget,
		Material,
		FVector(DecalThickness, Size, Size),
		Parameters.Location,
		DecalRotation,
		/*LifeSpan=*/0.f);

	if (!Decal)
	{
		return false;
	}

	Decal->SetFadeScreenSize(FadeScreenSize);
	Decal->SetFadeOut(
		FMath::Max(0.f, LifeSpan - FadeOutDuration),
		FMath::Max(0.f, FadeOutDuration),
		/*bDestroyOwnerAfterFade=*/true);

	return true;
}
