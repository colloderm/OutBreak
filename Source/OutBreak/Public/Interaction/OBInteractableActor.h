// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OBInteractableActor.generated.h"

class UWidgetComponent;
class UMaterialInterface;
class USphereComponent;
class UUserWidget;
class AOBPlayerController;

UCLASS()
class OUTBREAK_API AOBInteractableActor : public AActor
{
	GENERATED_BODY()

public:
	AOBInteractableActor();
	
	// 상호작용 실행. PC가 InteractWidgetClass를 열도록 위임
	UFUNCTION(BlueprintNativeEvent, Category = "Interaction")
	void Interact(AOBPlayerController* PC);
	virtual void Interact_Implementation(AOBPlayerController* PC);
	
	// 화면 안내 문구. 컨테이너는 비었을 때 다르게 답하려고 오버라이드한다.
	UFUNCTION(BlueprintNativeEvent, Category = "Interaction")
	FText GetInteractPromptText() const;
	virtual FText GetInteractPromptText_Implementation() const;

	// 현재 조준 대상 하나에만 외곽선을 켠다. 컨트롤러가 최근접 하나에만 호출한다.
	virtual void SetHighlighted(bool bHighlighted);
	
	// 문구만 다시 읽어 온다(대상은 그대로인데 내용이 바뀌는 경우: 상자를 다 털면 "비어 있음").
	void RefreshPromptText();

protected:
	virtual void BeginPlay() override;
	
	// 범위 진입/이탈 -> 컨트롤러에 "현재 상호작용 대상" 등록/해제
	UFUNCTION()
	void OnRangeBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep);
	UFUNCTION()
	void OnRangeEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
	
protected:
	UPROPERTY(VisibleAnywhere, Category = "Interaction")
	TObjectPtr<USphereComponent> Range;
	
	UPROPERTY(EditAnywhere, Category = "Interaction")
	TSubclassOf<UUserWidget> InteractWidgetClass;
	
	UPROPERTY(EditAnywhere, Category = "Interaction")
	float InteractRadius = 200.f;
	
	// 비워두면 "상호작용". 상자·NPC별로 에디터에서 채운다.
	UPROPERTY(EditAnywhere, Category = "Interaction")
	FText InteractPromptText;

	// 조준 대상일 때 메시 위에 덧씌우는 머티리얼. 비워두면 하이라이트가 없다.
	// 포스트프로세스 외곽선보다 싸고 프로젝트 세팅을 건드리지 않는다.
	UPROPERTY(EditAnywhere, Category = "Interaction")
	TObjectPtr<UMaterialInterface> HighlightOverlayMaterial;
	
	// 액터 머리 위 안내 문구. 위치·크기는 BP에서 이 컴포넌트를 옮겨 맞춘다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<UWidgetComponent> PromptWidgetComp;

};
