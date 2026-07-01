// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HordeProxyActor.generated.h"

UCLASS()
class OUTBREAK_API AHordeProxyActor : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AHordeProxyActor();
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<class UInstancedStaticMeshComponent> InstancedStaticMesh;
	
	
	int32 AddInstance(const FTransform& Transform);
	
	void RegisterInstances(
		TArray<int32>& InstanceIds,
		const TArray<FTransform>& Transforms) const;
	
	void RemoveInstance(const int32 InstanceId) const;
	
	void UpdateInstances(const TArray<FTransform>& Transforms) const;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};
