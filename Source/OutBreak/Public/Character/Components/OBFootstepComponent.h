// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Chaos/ChaosEngineInterface.h"   // EPhysicalSurface
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "OBFootstepComponent.generated.h"

class USkeletalMeshComponent;
class USoundAttenuation;
class USoundBase;

/**
 * 발소리 재생기. AnimNotify(UOBAnimNotify_Footstep)가 발 접지 프레임마다 호출한다.
 *
 * 왜 컴포넌트인가:
 * - 플레이어(AOBCharacterBase)와 좀비(AEnemyCharacter)는 공통 베이스가 없다(둘 다 ACharacter 직계).
 *   컴포넌트로 두면 노티파이 하나로 양쪽을 다 커버하고, 사운드/수치는 BP에서 한 번만 채운다.
 * - 노티파이는 UCLASS(const)라 상태를 못 가진다. 연타 방지 타임스탬프가 여기 있어야 한다.
 *
 * 순수 연출이다. 서버 판정에는 관여하지 않는다(데디 서버에서는 즉시 반환).
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class OUTBREAK_API UOBFootstepComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOBFootstepComponent();

	// 노티파이가 부르는 유일한 진입점(연출).
	// GASP의 BP_AnimNotify_FoleyEvent에서도 호출하므로 BP에 노출한다 —
	// GASP 애님에는 Foley 노티파이가 이미 전부 박혀 있어서, 그쪽에 얹는 편이
	// 모션 매칭 DB 전체에 우리 노티파이를 수작업으로 까는 것보다 압도적으로 싸다.
	// 반환값: 실제로 소리를 냈는가. 호출자가 자기 사운드를 건너뛸지 판단하는 데 쓴다.
	// bWarnIfNoSound: 사운드가 하나도 없을 때 경고할지. Foley 경로는 false를 넘긴다 —
	//   거기서는 "둘 다 비어 있음"이 오설정이 아니라 GASP에 넘긴다는 정상 구성이다.
	UFUNCTION(BlueprintCallable, Category = "Footstep")
	bool PlayFootstep(USkeletalMeshComponent* MeshComp, FName FootSocket, bool bWarnIfNoSound = true);

	// GASP Foley 노티파이 전용 진입점. BP_AnimNotify_FoleyEvent(부모)에서 부른다.
	// 부모를 고치면 Jump/Handplant 자식까지 전부 타므로, 어떤 이벤트가 발소리인지는
	// 여기서 거른다. BP에 Branch를 두지 않는 이유는 프로젝트 규칙(로직은 C++) 때문이다.
	// 반환 true = 우리가 소리를 냈으니 GASP Foley 사운드는 건너뛴다.
	// false = 표면 지정이 없는 바닥이거나 발소리 이벤트가 아니므로 Foley가 담당한다.
	UFUNCTION(BlueprintCallable, Category = "Footstep")
	bool PlayFootstepFromFoley(USkeletalMeshComponent* MeshComp, FGameplayTag FoleyEvent, FName FootSocket);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 표면별 발소리. 밟은 표면이 여기 없거나 값이 비어 있으면 아래 FootstepSound로 떨어진다.
	// 표면 종류는 Config/DefaultEngine.ini의 [/Script/Engine.PhysicsSettings] 정의를 따른다.
	// 비워두면 지금과 똑같이 동작한다 — 흙/콘크리트만 먼저 넣어도 된다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep")
	TMap<TEnumAsByte<EPhysicalSurface>, TObjectPtr<USoundBase>> SurfaceSounds;

	// 폴백 발소리. 표면이 미지정이거나 위 맵에 없을 때 쓴다.
	// 이것까지 비어 있으면 아무 소리도 나지 않는다(로그로 알린다).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep")
	TObjectPtr<USoundBase> FootstepSound;

	// 발소리로 취급할 Foley 이벤트. 비워두면 BeginPlay가 기본값을 채운다
	// (Walk/WalkBackwds/Run/RunBackwds/RunStrafe/Scuff/ScuffPivot/Land).
	// Jump·Handplant·ScuffWall·Slide.Loop은 바닥 표면음이 어울리지 않아 제외한다.
	// 태그 목록은 Config/DefaultGameplayTags.ini의 Foley.Event.* 참조.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep|Foley")
	FGameplayTagContainer FoleyFootstepEvents;

	// 발 소켓 기준 위아래로 이 거리(cm)만큼 훑어 바닥을 찾는다.
	// 애니메이션에 따라 발이 지면에서 살짝 뜨거나 파묻히므로 양방향으로 잡는다.
	// SurfaceSounds가 비어 있으면 트레이스 자체를 건너뛴다(비용 0).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep", meta = (ClampMin = "1.0"))
	float GroundTraceHeight = 40.f;

	// 거리 감쇠. 비우면 사운드 애셋 자체의 감쇠 설정을 따른다.
	// PvP에서 "얼마나 멀리서 들리는가"가 곧 밸런스라 여기서 잡는 걸 권장한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep")
	TObjectPtr<USoundAttenuation> Attenuation;

	// 걷기 기준 볼륨.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep|Volume", meta = (ClampMin = "0.0"))
	float WalkVolume = 1.f;

	// 웅크림. 0으로 두면 완전 무음 이동이 된다 — 밸런스상 0보다는 낮은 값을 권장.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep|Volume", meta = (ClampMin = "0.0"))
	float CrouchVolume = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep|Volume", meta = (ClampMin = "0.0"))
	float SprintVolume = 1.6f;

	// 이 속도(cm/s) 이상이면 스프린트로 본다. GASP의 달리기(500)와 스프린트(700) 사이에 둘 것.
	// gait 플래그 대신 실제 속도를 쓰는 이유: 모든 머신에서 같은 값이 나오고 GASP 내부에 의존하지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep|Gait", meta = (ClampMin = "0.0"))
	float SprintSpeedThreshold = 650.f;

	// 이 속도 미만이면 발소리를 내지 않는다. 벽에 밀착해 제자리걸음 할 때 울리는 걸 막는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep|Gait", meta = (ClampMin = "0.0"))
	float MinSpeedThreshold = 20.f;

	// 연속 발소리 최소 간격(초). 애니 블렌드 구간에서 두 시퀀스의 노티파이가
	// 같은 프레임에 겹쳐 발화하는 걸 막는다.
	// ★ 0.15는 너무 길었다 — 700cm/s 스프린트는 발당 약 0.11초라 정상 걸음이 잘린다.
	//   겹침 방지는 프레임 단위면 충분하므로 넉넉히 낮춘다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep", meta = (ClampMin = "0.0"))
	float MinInterval = 0.08f;

	// 매번 같은 소리로 들리지 않게 피치를 흔든다. 0이면 고정.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float PitchJitter = 0.08f;

	//~ AI 소음 (서버 전용) ------------------------------------------------
	// 연출과 완전히 분리된 경로다. 노티파이를 쓰지 않는 이유:
	//  - 데디 서버에서 애니 그래프 노티파이 발화가 보장되지 않는다.
	//  - 클라가 RPC로 보고하게 하면 조작 클라가 보고를 빼고 무음 이동을 할 수 있다.
	// 그래서 서버가 이동 거리를 직접 누적해 보폭마다 발생시킨다.
	//
	// Director(UOutBreakGlobal::ReportNoiseToAI)를 타지 않고 AI 인지(Hearing)에
	// 직접 보고한다. Director는 소음 1건마다 DefaultResponders 만큼 좀비를 '증원 스폰'
	// 하도록 설계돼 있어서(총성처럼 드문 사건 기준) 발소리 같은 연속 자극에는 맞지 않는다.
	// Loudness를 0으로 내려도 max(0.25f, ...) 바닥에 걸려 2마리 밑으로 못 내려간다.
	//   총성  → Director: 재배치 + 증원 스폰 (escalate)
	//   발소리 → Hearing:  주변 좀비가 알아챌 뿐, 스폰 없음

	// 발소리로 좀비를 부를지. 끄면 타이머 자체가 돌지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep|Noise")
	bool bEmitAINoise = true;

	// 웅크림 이동 시에도 소음을 낼지. 기본 false — 웅크림을 스텔스 수단으로 남긴다.
	// 켜도 '작은 소음'은 못 만든다: Director가 Loudness를 0.25로 바닥 처리한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep|Noise")
	bool bEmitNoiseWhileCrouched = false;

	// 이 거리(cm)를 이동할 때마다 소음 1회. 좀비가 위치를 갱신하는 주기이기도 하다.
	// 짧게 잡을수록 추적이 촘촘해지지만 인지 갱신 횟수도 늘어난다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep|Noise", meta = (ClampMin = "1.0"))
	float NoiseStrideDistance = 600.f;

	// 소음 강도. 청취 반경 = min(좀비의 HearingRange × Loudness, 아래 Range).
	// EnemyController의 HearingRange가 10000이므로 0.15면 1500cm에 해당한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep|Noise", meta = (ClampMin = "0.0"))
	float WalkNoiseLoudness = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep|Noise", meta = (ClampMin = "0.0"))
	float SprintNoiseLoudness = 0.6f;

	// 소음이 닿는 반경(cm). 상한이다 — Loudness로 계산된 값과 둘 중 작은 쪽이 적용된다.
	// ★ 0 이하로 두면 상한이 사라져 HearingRange(10000)까지 들린다. 조용하게 만들려고
	//   0을 넣으면 안 된다. 줄이려면 이 값을 작게 잡을 것.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep|Noise", meta = (ClampMin = "1.0"))
	float WalkNoiseRange = 1500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep|Noise", meta = (ClampMin = "1.0"))
	float SprintNoiseRange = 3000.f;

	// 서버 샘플링 주기(초). 이동 거리 누적용이라 촘촘할 필요 없다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep|Noise", meta = (ClampMin = "0.02"))
	float NoiseSampleInterval = 0.1f;

private:
	void SampleFootstepNoise();

	// 발 밑을 훑어 밟은 표면을 알아낸다. 못 찾으면 SurfaceType_Default.
	EPhysicalSurface TraceGroundSurface(const FVector& FootLocation) const;

	// 마지막 재생 시각(월드 시간). 연타 방지용.
	float LastFootstepTime = -1.f;

	// 사운드 미지정 경고를 한 번만 남기기 위한 플래그.
	bool bWarnedMissingSound = false;

	FTimerHandle NoiseSampleTimer;
	FVector LastNoiseSampleLocation = FVector::ZeroVector;
	float AccumulatedStrideDistance = 0.f;
};
