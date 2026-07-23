// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AI/Data/EnemyAsset.h"
#include "AI/Interface/EnemyComponentInterface.h"
#include "EnemyBaseActorComponent.generated.h"


class AEnemyCharacter;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class OUTBREAK_API UEnemyBaseActorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UEnemyBaseActorComponent();
	
	AEnemyCharacter* GetEnemyCharacter();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	
	UPROPERTY(BlueprintReadOnly, Category = "Asset", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEnemyAsset> EnemyAsset = nullptr;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;
	
private:
	UPROPERTY(meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AEnemyCharacter> EnemyCharacter;
	
	
	
};
