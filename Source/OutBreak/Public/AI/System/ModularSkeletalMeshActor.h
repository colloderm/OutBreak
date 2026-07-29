// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ModularSkeletalMeshActor.generated.h"

class USkeletalMeshComponent;

UCLASS()
class OUTBREAK_API AModularSkeletalMeshActor : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AModularSkeletalMeshActor();
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite);
	TObjectPtr<class USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite);
	TObjectPtr<class USkeletalMeshComponent> LeaderHead;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};
