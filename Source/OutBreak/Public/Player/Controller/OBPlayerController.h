// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Game/Expedition/OBExpeditionTypes.h"
#include "OBPlayerController.generated.h"

class UWorld;
class AOBInteractableActor;
class UUserWidget;
class AOBWeaponBase;
enum class EOBWeaponSlot : uint8;
class UOBAbilitySystemComponent;
class UCameraShakeBase;
class UOBInputConfig;
struct FGameplayTag;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;

UCLASS()
class OUTBREAK_API AOBPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	AOBPlayerController();
	
	//발사 시 시야 회전 반동 + 카메라 쉐이크 적용.
	void ApplyWeaponRecoil(float PitchKick, float YawKick, float RecoverySpeed, TSubclassOf<UCameraShakeBase> CameraShake, float CameraShakeScale = 1.f);

protected:
	virtual void Tick(float DeltaSeconds) override;

private:
	// 누적된 반동을 매 프레임 0으로 복귀시킨다.
	void UpdateRecoilRecovery(float DeltaSeconds);

private:
	// 아직 복구 안 된 반동 누적량.
	float AccumulatedRecoilPitch = 0.0f;
	float AccumulatedRecoilYaw = 0.0f;

	// 현재 무기의 복구 속도(ApplyWeaponRecoil에서 갱신).
	float CurrentRecoilRecoverySpeed = 8.0f;
	
	// 현재 상호작용 가능한 대상(범위 안). 약참조.
	TWeakObjectPtr<AOBInteractableActor> CurrentInteractable;

	// 현재 열려있는 상호작용 위젯(중복 오픈 방지 + 닫기용).
	UPROPERTY()
	TObjectPtr<UUserWidget> ActiveInteractionWidget;
	
protected:
	//~ APlayerController interface
	virtual void SetupInputComponent() override;
	virtual void BeginPlay() override;
	//~ End interface
	
	// 클라: PlayerState가 복제되어 들어온 시점 → 사망상태 구독.
	virtual void OnRep_PlayerState() override;
	
	void Input_Move(const FInputActionValue& Value);
	void Input_Look(const FInputActionValue& Value);
	void Input_JumpStarted();
	void Input_JumpCompleted();
	
	// 능력 입력 핸들러(눌림/뗌).
	void Input_AbilityInputPressed(FGameplayTag InputTag);
	void Input_AbilityInputReleased(FGameplayTag InputTag);

	// 조종 폰의 커스텀 ASC 조회.
	UOBAbilitySystemComponent* GetOBAbilitySystemComponent() const;
	
	void Input_EquipSlot(EOBWeaponSlot Slot);
	
	virtual void AcknowledgePossession(APawn* P) override;
	
	void Input_Interact();
	
	void Input_TogglePartyUI();
	
	//~ Expedition 사망 피드백 ---------------------------------
	void BindToExpeditionStatus();          // 로컬 PS 상태변경 구독(중복가드)
	void HandleExpeditionStatusChanged();   // 상태 → 화면 처리
	void ShowDeathScreen();
	void ShowExtractScreen();
	void HideDeathScreen();
	
	void ShowSpectatorHUD();
	void HideSpectatorHUD();
	
	//~ Expedition 세션 종료(결과 → Home 복귀) --------------------
	void BindToGameStatePhase();   // GameState 준비 대기 후 페이즈 구독
	UFUNCTION()
	void HandleExpeditionPhaseChanged(EOBExpeditionPhase NewPhase);
	void ShowResultScreen();
	
protected:
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	int32 InputMappingPriority = 0;
	
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UOBInputConfig> InputConfig;
	
	UPROPERTY(EditDefaultsOnly, Category = "Input|Weapon")
	TObjectPtr<UInputAction> SlotPrimaryAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input|Weapon")
	TObjectPtr<UInputAction> SlotSecondaryAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input|Weapon")
	TObjectPtr<UInputAction> SlotMeleeAction;
	
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> InteractAction;
	
	// 파티 UI 토글 입력.
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> PartyToggleAction;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UUserWidget> PartyWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> PartyWidget;
	
	// 사망 시 띄울 위젯(WBP_DeathScreen). 미지정이면 입력잠금만 수행.
	UPROPERTY(EditDefaultsOnly, Category = "Expedition")
	TSubclassOf<UUserWidget> DeathScreenWidgetClass;
	
	UPROPERTY(EditDefaultsOnly, Category = "Expedition")
	TSubclassOf<UUserWidget> ExtractScreenWidgetClass;
	
	UPROPERTY(EditDefaultsOnly, Category = "Expedition") 
	TSubclassOf<UUserWidget> ExtractionProgressWidgetClass;
	
	// 세션 종료 결과 위젯(WBP_ExpeditionResult).
	UPROPERTY(EditDefaultsOnly, Category = "Expedition")
	TSubclassOf<UUserWidget> ResultWidgetClass;

	// 복귀할 Home 레벨(L_HomeMap).
	UPROPERTY(EditDefaultsOnly, Category = "Expedition")
	TSoftObjectPtr<UWorld> HomeLevel;
	
	// 사망/탈출 화면을 보여주는 시간. 이 시간이 지나야 결과창으로 넘어간다.
	UPROPERTY(EditDefaultsOnly, Category = "Expedition")
	float ResultDelaySeconds = 3.f;

	// 결과창 후 자동 Home 복귀까지 대기(초). 0이면 버튼으로만.
	UPROPERTY(EditDefaultsOnly, Category = "Expedition")
	float AutoReturnSeconds = 10.f;

	// 현재 결과 위젯(중복 방지).
	UPROPERTY()
	TObjectPtr<UUserWidget> ActiveResultWidget;

	bool bPhaseBound = false;              // 페이즈 중복 바인딩 방지
	FTimerHandle PhaseBindRetryTimer;      // GameState 대기 재시도
	FTimerHandle AutoReturnTimer;          // 자동 복귀
	FTimerHandle ResultDelayTimer;         // 사망/탈출 화면 → 결과창 전환 지연
	
	// 결과창 구독 재시도 횟수. 원정 GameState가 없는 맵에서 4Hz 타이머가 영원히 도는 걸 막는다(0.25초 × 40 = 10초).
	int32 PhaseBindAttempts = 0;
	static constexpr int32 MaxPhaseBindAttempts = 40;

	// 현재 떠 있는 사망 위젯(중복 방지/제거).
	UPROPERTY()
	TObjectPtr<UUserWidget> ActiveDeathWidget;
	
	// 관전 중 표시할 위젯(WBP_SpectatorHUD).
	UPROPERTY(EditDefaultsOnly, Category = "Expedition")
	TSubclassOf<UUserWidget> SpectatorHUDWidgetClass;

	UPROPERTY()
	TObjectPtr<UUserWidget> ActiveSpectatorWidget;

	// PS 델리게이트 중복 바인딩 방지 플래그.
	bool bExpeditionStatusBound = false;
	
public:
	UFUNCTION(Server, Reliable) 
	void Server_SetWeaponSlot(EOBWeaponSlot Slot, TSubclassOf<AOBWeaponBase> WeaponClass);
	
	UFUNCTION(Server, Reliable) 
	void Server_SetReady(bool bReady);
	
	UFUNCTION(Server, Reliable) 
	void Server_StartGame();
	
	// 클라 → 서버: 내 GameInstance Loadout을 PlayerState로 적용.
	UFUNCTION(Server, Reliable)
	void Server_ApplyLoadout(const TArray<TSubclassOf<AOBWeaponBase>>& Weapons);
	
	// 파티 리더십을 서버 PlayerState에 반영(게이팅용).
	UFUNCTION(Server, Reliable)
	void Server_SetPartyLeader(bool bLeader);
	
	//~ 디버그 -----------------------------------------------------
	// 콘솔(`)에 OBSuicide 입력 → 즉시 사망. 사망/관전/전멸 흐름 테스트용.
	UFUNCTION(Exec)
	void OBSuicide();
	
	UFUNCTION(Server, Reliable)
	void Server_Suicide();
	
	// 상호작용 위젯 오픈/클로즈(커서·UIOnly·이동잠금을 여기서 일괄 처리).
	UFUNCTION(BlueprintCallable)
	UUserWidget* OpenInteractionWidget(TSubclassOf<UUserWidget> WidgetClass);
	void CloseInteractionWidget();

	// 범위 내 상호작용 대상 등록(액터가 호출).
	void SetCurrentInteractable(AOBInteractableActor* Interactable);
	AOBInteractableActor* GetCurrentInteractable() const;
	
	//~ Expedition 관전 -------------------------------------------
	// 서버 → 클라: 관전 시작(팀원 생존).
	UFUNCTION(Client, Reliable)
	void ClientBeginSpectate();

	// 서버 → 클라: 팀 전멸 → 관전 종료 + 사망(홈 복귀) 화면.
	UFUNCTION(Client, Reliable)
	void ClientTeamWiped();

	// 클라 → 서버: 관전 대상 순환. 후보는 서버가 정하므로 적 팀은 볼 수 없다.
	UFUNCTION(Server, Reliable)
	void ServerCycleSpectateTarget(int32 Direction);
	
	// 관전 시점 전환. 서버 SetViewTarget은 복제되지 않으므로 클라 통지를 반드시 같이 보낸다.
	void SetSpectateViewTarget(AActor* NewTarget);

	// 관전 HUD 버튼용.
	UFUNCTION(BlueprintCallable, Category = "Expedition")
	void SpectateNext() { ServerCycleSpectateTarget(+1); }

	UFUNCTION(BlueprintCallable, Category = "Expedition")
	void SpectatePrev() { ServerCycleSpectateTarget(-1); }

	// 관전 HUD의 "관전 중: <이름>" 표시용.
	UFUNCTION(BlueprintPure, Category = "Expedition")
	FString GetSpectateTargetName() const;
	
	// 결과창 버튼/자동타이머가 호출 → 데디 종료 후 로컬 Home 로드.
	UFUNCTION(BlueprintCallable, Category = "Expedition")
	void ReturnToHome();
	
	// 자동 복귀까지 남은 초(올림). 결과창 버튼 텍스트 "복귀하기 (N)"용.
	UFUNCTION(BlueprintPure, Category = "Expedition")
	int32 GetReturnCountdown() const;
};
