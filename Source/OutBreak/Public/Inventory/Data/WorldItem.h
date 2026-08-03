// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Inventory/Data/InventoryData.h"
#include "WorldItem.generated.h"

UCLASS()
class OUTBREAK_API AWorldItem : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AWorldItem();
	
	FWorldItemData* GetWorldItemData() { return &ItemData; }



protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	void PickUpCompleted();
	
	friend class UPlayerInventoryComponent;
	
public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
private:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	FWorldItemData ItemData;
};
