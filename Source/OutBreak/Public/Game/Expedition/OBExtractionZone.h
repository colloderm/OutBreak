#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Game/Expedition/OBExpeditionTypes.h"
#include "OBExtractionZone.generated.h"

class USphereComponent;
class AOBExpeditionGameState;

/**
 * 탈출 구역. 살아있는 플레이어가 HoldTime 동안 구역에 머물면 탈출 성공.
 * - 서버 전용 로직(레벨 배치 액터, 각 머신에 존재하지만 판정은 서버만).
 * - 진행도는 PlayerState.ExtractionProgress로 소유 클라에 복제되어 HUD에 표시.
 */
UCLASS()
class OUTBREAK_API AOBExtractionZone : public AActor
{
	GENERATED_BODY()

public:
	AOBExtractionZone();
	
	// 개인(팀 전용) 탈출구로 설정. 같은 팀 전원에게만 복제되어 보인다.
	void ConfigureAsPersonal(uint8 InTeamId);
	
	// 팀원 전원이 봐야 하므로 소유자 한정 복제 대신 팀으로 거른다.
	virtual bool IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const override;
	
	// 활성 시간창 설정. GameMode가 개인 존 스폰 시 맵(MapData)별 값으로 덮어씀.
	void SetActiveWindow(int32 InStartSec, int32 InEndSec);
	
	// 탈출구 활성/비활성 전환 시 호출. BP에서 비콘 이펙트 표시/숨김 구현.
	UFUNCTION(BlueprintImplementableEvent, Category = "Extraction")
	void OnExtractionActiveChanged(bool bActive);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep);

	UFUNCTION()
	void OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	// 현재 경과시간 기준으로 이 탈출구가 활성인지.
	bool IsActiveNow() const;

	// 이 플레이어가 사용 가능한지(개인 탈출 필요아이템 등). 현재 스텁(true).
	bool CanPlayerExtract(AController* C) const;

	AOBExpeditionGameState* GetExpeditionGameState() const;
	
	void CheckActiveState(); // 활성창 경계에서 이펙트 토글

protected:
	UPROPERTY(VisibleAnywhere, Category = "Extraction")
	TObjectPtr<USphereComponent> Trigger;

	// 개인/공용.
	UPROPERTY(EditAnywhere, Category = "Extraction")
	EOBExtractionType ExtractType = EOBExtractionType::Public;

	// 탈출까지 머물러야 하는 시간(초).
	UPROPERTY(EditAnywhere, Category = "Extraction", meta = (ClampMin = "0.1"))
	float HoldTime = 8.f;

	// 활성 시작 경과초(0=처음부터).
	UPROPERTY(EditAnywhere, Category = "Extraction", meta = (ClampMin = "0"))
	int32 ActiveStartSec = 0;

	// 활성 종료 경과초(0=끝까지). 개인은 예: 600.
	UPROPERTY(EditAnywhere, Category = "Extraction", meta = (ClampMin = "0"))
	int32 ActiveEndSec = 0;

	// 개인 탈출 필요아이템 태그(선택). 인벤토리 붙기 전엔 미사용.
	UPROPERTY(EditAnywhere, Category = "Extraction")
	FGameplayTag RequiredItemTag;

private:
	// 0 = 팀 제한 없음(공용). 그 외에는 이 팀에게만 보이고 이 팀만 사용 가능.
	uint8 OwningTeamId = 0;
	
	// 서버 전용: 구역 안 컨트롤러 → 누적 홀드(초).
	UPROPERTY(Transient)
	TMap<TObjectPtr<AController>, float> HoldProgress;
	
	FTimerHandle ActiveCheckTimer;
	bool bLastActiveState = false;
};
