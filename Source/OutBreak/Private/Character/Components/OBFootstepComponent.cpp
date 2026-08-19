// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/Components/OBFootstepComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Perception/AISense_Hearing.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

namespace
{
	// EnemyMemoryComponent가 자극 출처를 구분할 수 있게 태그를 붙인다.
	const FName FootstepNoiseTag(TEXT("Footstep"));
}

UOBFootstepComponent::UOBFootstepComponent()
{
	// 연출은 노티파이가 밀어 주고, 소음은 타이머가 돌린다. 틱은 필요 없다.
	PrimaryComponentTick.bCanEverTick = false;
}

void UOBFootstepComponent::BeginPlay()
{
	Super::BeginPlay();

	// 생성자가 아니라 여기서 채운다. ini로 선언된 태그는 GameplayTagsManager가
	// 로드한 뒤에야 유효한데, CDO 생성 시점에는 그 보장이 없다.
	// ★ 아래 AI 소음 조기 반환보다 먼저 와야 한다. 소음을 끈 캐릭터에서도 필요하다.
	if (FoleyFootstepEvents.IsEmpty())
	{
		const FName DefaultEvents[] = {
			TEXT("Foley.Event.Walk"),
			TEXT("Foley.Event.WalkBackwds"),
			TEXT("Foley.Event.Run"),
			TEXT("Foley.Event.RunBackwds"),
			TEXT("Foley.Event.RunStrafe"),
			TEXT("Foley.Event.Scuff"),
			TEXT("Foley.Event.ScuffPivot"),
			TEXT("Foley.Event.Land")
		};

		for (const FName& TagName : DefaultEvents)
		{
			const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TagName, /*ErrorIfNotFound=*/false);
			if (Tag.IsValid())
			{
				FoleyFootstepEvents.AddTag(Tag);
			}
		}
	}

	const AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!bEmitAINoise || !Owner || !Owner->HasAuthority() || !World)
	{
		return;
	}

	// 첫 샘플에서 큰 델타가 잡히지 않도록 시작 위치를 미리 채운다.
	LastNoiseSampleLocation = Owner->GetActorLocation();
	AccumulatedStrideDistance = 0.f;

	World->GetTimerManager().SetTimer(
		NoiseSampleTimer,
		this,
		&UOBFootstepComponent::SampleFootstepNoise,
		FMath::Max(0.02f, NoiseSampleInterval),
		true);
}

void UOBFootstepComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(NoiseSampleTimer);
	}
	Super::EndPlay(EndPlayReason);
}

void UOBFootstepComponent::SampleFootstepNoise()
{
	const ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (!IsValid(OwnerCharacter))
	{
		return;
	}

	// 공중이거나 래그돌(사망)이면 발이 땅에 없다. 누적도 끊는다.
	const UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement();
	if (!Movement || !Movement->IsMovingOnGround())
	{
		AccumulatedStrideDistance = 0.f;
		LastNoiseSampleLocation = OwnerCharacter->GetActorLocation();
		return;
	}

	const FVector CurrentLocation = OwnerCharacter->GetActorLocation();
	const float StepDistance = FVector::Dist2D(CurrentLocation, LastNoiseSampleLocation);
	LastNoiseSampleLocation = CurrentLocation;

	// 실제로 이동 중일 때만 누적한다. 밀려나거나 제자리걸음이면 초기화.
	if (OwnerCharacter->GetVelocity().Size2D() < MinSpeedThreshold)
	{
		AccumulatedStrideDistance = 0.f;
		return;
	}

	const bool bCrouched = OwnerCharacter->bIsCrouched;
	if (bCrouched && !bEmitNoiseWhileCrouched)
	{
		// 누적까지 끊는다. 안 그러면 웅크려 접근한 뒤 일어서는 순간 한 번에 터진다.
		AccumulatedStrideDistance = 0.f;
		return;
	}

	AccumulatedStrideDistance += StepDistance;
	if (AccumulatedStrideDistance < NoiseStrideDistance)
	{
		return;
	}
	AccumulatedStrideDistance = 0.f;

	const bool bSprinting = !bCrouched &&
		OwnerCharacter->GetVelocity().Size2D() >= SprintSpeedThreshold;
	const float Loudness = bSprinting ? SprintNoiseLoudness : WalkNoiseLoudness;
	const float Range = bSprinting ? SprintNoiseRange : WalkNoiseRange;

	// Range를 0으로 두면 상한이 사라져 HearingRange 전체까지 들린다.
	// 여기서 끊어야 "0 = 무음"이라는 상식적 기대가 지켜진다.
	if (Loudness <= 0.f || Range <= 0.f)
	{
		return;
	}

	// Director(ReportNoiseToAI)가 아니라 AI 인지에 직접 보고한다.
	// Director는 소음 1건마다 좀비를 증원 스폰하므로 발소리처럼 계속 발생하는
	// 자극에 쓰면 플레이어가 좀비를 끌고 다니게 된다. 총성만 그 경로를 쓴다.
	AActor* Owner = GetOwner();
	UAISense_Hearing::ReportNoiseEvent(
		Owner,
		CurrentLocation,
		Loudness,
		Owner,
		Range,
		FootstepNoiseTag);

	UE_LOG(LogTemp, Verbose,
		TEXT("[Footstep] 소음 보고 %s Loudness=%.2f Range=%.0f"),
		*GetNameSafe(Owner), Loudness, Range);
}

bool UOBFootstepComponent::PlayFootstepFromFoley(
	USkeletalMeshComponent* MeshComp,
	const FGameplayTag FoleyEvent,
	const FName FootSocket)
{
	// 부모 노티파이 하나를 고치면 자식 10개가 전부 여기로 들어온다.
	// 점프·손짚기·벽 스커프까지 발소리를 내면 안 되므로 여기서 거른다.
	if (!FoleyFootstepEvents.HasTagExact(FoleyEvent))
	{
		return false;
	}

	return PlayFootstep(MeshComp, FootSocket, /*bWarnIfNoSound=*/false);
}

EPhysicalSurface UOBFootstepComponent::TraceGroundSurface(const FVector& FootLocation) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return SurfaceType_Default;
	}

	// 발이 지면 위로 뜨거나 아래로 파묻힌 프레임이 모두 있어서 양방향으로 훑는다.
	const FVector Start = FootLocation + FVector(0.f, 0.f, GroundTraceHeight);
	const FVector End = FootLocation - FVector(0.f, 0.f, GroundTraceHeight);

	// ★ bTraceComplex를 끄면 안 된다. 심플 콜리전에 맞으면 머티리얼별
	//   PhysicalMaterial이 아니라 메시의 Simple Collision Physical Material이
	//   돌아온다. 바닥은 대개 심플 박스라 머티리얼 할당이 통째로 무시된다.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(OBFootstepSurface), /*bTraceComplex=*/true, GetOwner());
	Params.bReturnPhysicalMaterial = true;

	// 바닥이 Visibility 채널에 Block이 아니면 미명중이 된다. 총기 트레이스는
	// OB_TraceChannel_Weapon을 쓰므로, 데칼은 표면이 잡히는데 발소리만 안 잡히면
	// 그 바닥의 Visibility 응답을 볼 것.
	FHitResult Hit;
	if (!World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		return SurfaceType_Default;
	}

	return UPhysicalMaterial::DetermineSurfaceType(Hit.PhysMaterial.Get());
}

bool UOBFootstepComponent::PlayFootstep(
	USkeletalMeshComponent* MeshComp,
	const FName FootSocket,
	const bool bWarnIfNoSound)
{
	if (!IsValid(MeshComp))
	{
		return false;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// 데디 서버는 렌더도 청취자도 없다. 애니 노티파이가 데디에서 발화하는지는
	// 메시의 VisibilityBasedAnimTickOption에 따라 달라지므로, 그 설정에 의존하지 않도록
	// 여기서 명시적으로 끊는다. 이 컴포넌트는 순수 연출이다.
	if (World->GetNetMode() == NM_DedicatedServer)
	{
		return false;
	}

	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
	{
		return false;
	}

	// 실제로 이동 중일 때만 낸다. 제자리걸음 애니메이션에서 울리는 걸 막는다.
	const float Speed = OwnerPawn->GetVelocity().Size2D();
	if (Speed < MinSpeedThreshold)
	{
		return false;
	}

	// 블렌드 구간에서 두 시퀀스의 노티파이가 같은 프레임에 겹칠 수 있다.
	const float Now = World->GetTimeSeconds();
	if (LastFootstepTime >= 0.f && Now - LastFootstepTime < MinInterval)
	{
		return false;
	}
	LastFootstepTime = Now;

	// 웅크림이 스프린트보다 우선한다. 웅크린 채로 임계속도를 넘길 일은 없지만
	// 무기 기동성 배율이 붙으면 경계에서 흔들릴 수 있어 순서를 고정한다.
	float Volume = WalkVolume;
	const ACharacter* OwnerCharacter = Cast<ACharacter>(OwnerPawn);
	if (OwnerCharacter && OwnerCharacter->bIsCrouched)
	{
		Volume = CrouchVolume;
	}
	else if (Speed >= SprintSpeedThreshold)
	{
		Volume = SprintVolume;
	}

	if (Volume <= 0.f)
	{
		return false;
	}

	// 소켓이 없으면 액터 원점으로 떨어뜨린다. 스켈레톤 본 이름이 틀려도 소리는 나야
	// "왜 안 들리지"가 아니라 "위치가 이상하다"로 증상이 드러난다.
	const FVector FootLocation = MeshComp->DoesSocketExist(FootSocket)
		? MeshComp->GetSocketLocation(FootSocket)
		: OwnerPawn->GetActorLocation();

	// 표면 판정. 맵이 비어 있으면 트레이스를 아예 쏘지 않는다 — 표면별 사운드를
	// 안 쓰는 캐릭터(좀비 등)에 트레이스 비용을 물리지 않기 위해서다.
	USoundBase* Sound = FootstepSound;
	if (SurfaceSounds.Num() > 0)
	{
		const EPhysicalSurface Surface = TraceGroundSurface(FootLocation);
		if (const TObjectPtr<USoundBase>* Found = SurfaceSounds.Find(Surface))
		{
			if (*Found)
			{
				Sound = *Found;
			}
		}
	}

	if (!Sound)
	{
		// Foley 경로(bWarnIfNoSound=false)에서는 둘 다 비어 있는 게 정상 구성이다.
		// GASP가 전부 담당하고 우리는 표면이 지정된 바닥에서만 끼어든다.
		// 경고는 Foley가 없는 경로 — 좀비(UOBAnimNotify_Footstep) — 에만 의미가 있다.
		if (bWarnIfNoSound && !bWarnedMissingSound)
		{
			bWarnedMissingSound = true;
			UE_LOG(LogTemp, Warning,
				TEXT("[Footstep] %s 의 FootstepComponent에 Footstep Sound가 비어 있다. "
					 "캐릭터 BP의 Footstep 컴포넌트에 사운드를 지정할 것."),
				*GetNameSafe(GetOwner()));
		}
		return false;
	}

	const float Pitch = 1.f + FMath::FRandRange(-PitchJitter, PitchJitter);

	UGameplayStatics::PlaySoundAtLocation(
		this,
		Sound,
		FootLocation,
		FRotator::ZeroRotator,
		Volume,
		Pitch,
		0.f,
		Attenuation);

	return true;
}
