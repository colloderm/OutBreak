// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EnemyBaseActorComponent.h"
#include "EnemyStatusComponent.generated.h"


UENUM(Blueprintable)
enum class Limb : uint8
{
	Head,
};

USTRUCT(BlueprintType)
struct FLimbData
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsHas = true;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxDurability;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Durability;
	
	FLimbData() = default;
	
	FLimbData(bool inHas, float inMaxDurability, float inDurability)
		: bIsHas(inHas), MaxDurability(inMaxDurability), Durability(inDurability) {}
	
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class OUTBREAK_API UEnemyStatusComponent : public UEnemyBaseActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UEnemyStatusComponent();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsDrawDebug;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLimbData Head = FLimbData(true, 100, 100);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLimbData Body = FLimbData(true, 100, 100) ;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLimbData Arm_R = FLimbData(true, 100, 100);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLimbData Arm_L = FLimbData(true, 100, 100);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLimbData Leg_R = FLimbData(true, 100, 100);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLimbData Leg_L = FLimbData(true, 100, 100);

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;
	
private:
	
	void DrawDebug();
};
