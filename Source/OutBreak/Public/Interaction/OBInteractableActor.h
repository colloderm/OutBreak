// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OBInteractableActor.generated.h"

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

};
