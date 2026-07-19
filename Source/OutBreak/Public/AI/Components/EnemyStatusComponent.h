// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EnemyBaseActorComponent.h"
#include "EnemyStatusComponent.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class OUTBREAK_API UEnemyStatusComponent : public UEnemyBaseActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UEnemyStatusComponent();
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bIsHasArm_R;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bIsHasArm_L;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bIsHasLeg_R;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bIsHasArm_R;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;
};
