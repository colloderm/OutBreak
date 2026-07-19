// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/Components/EnemyStatusComponent.h"
#include "AI/EnemyCharacter.h"

// Sets default values for this component's properties
UEnemyStatusComponent::UEnemyStatusComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UEnemyStatusComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UEnemyStatusComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	
	
	
	DrawDebug();

	// ...
}

void UEnemyStatusComponent::DrawDebug()
{
	if (bIsDrawDebug)
	{
		UWorld* World = GetWorld();
		if (!IsValid(World))
		{
			UE_LOG(LogTemp, Error, TEXT("%s::%s : World is invalid."), *GetClass()->GetName(), TEXT(__FUNCTION__));
			return;
		}
		
		AEnemyCharacter* Character = GetEnemyCharacter();
		if (!IsValid(Character))
		{
			UE_LOG(LogTemp, Error, TEXT("%s::%s : Character is invalid."), *GetClass()->GetName(), TEXT(__FUNCTION__));
			return;
		}
		
		USkeletalMeshComponent* MeshComp = Character->GetChildActorSkeletalMesh();
		if (!IsValid(MeshComp))
		{
			UE_LOG(LogTemp, Error, TEXT("%s::%s : Mesh Component is invalid."), *GetClass()->GetName(), TEXT(__FUNCTION__));
			return; 
		}
		
		
		const FVector ActorPos = Character->GetActorLocation();
		
		const FVector Head_Pos = MeshComp->GetSocketLocation("Head");
		const FVector Body_Pos = MeshComp->GetSocketLocation("spine_02");
		const FVector ArmR_Pos = MeshComp->GetSocketLocation("upperarm_r");
		const FVector ArmL_Pos = MeshComp->GetSocketLocation("upperarm_l");
		const FVector LegR_Pos = MeshComp->GetSocketLocation("thigh_r");
		const FVector LegL_Pos = MeshComp->GetSocketLocation("thigh_l");
		const float DT = 0.f;
		
		FLimbData* Data = &Head;
		FVector Pos = Head_Pos;
		if (Data->bIsHas)
		{
			DrawDebugString(World, Pos, FString::Printf(TEXT("Head : %.01f / %.01f"), Data->Durability, Data->MaxDurability), nullptr, FColor::White, DT);
		}
		
		Data = &Body;
		Pos = Body_Pos;
		if (Data->bIsHas)
		{
			DrawDebugString(World, Pos, FString::Printf(TEXT("Body : %.01f / %.01f"), Data->Durability, Data->MaxDurability), nullptr, FColor::White, DT);
		}
		
		Data = &Arm_R;
		Pos = ArmR_Pos;
		if (Data->bIsHas)
		{
			DrawDebugString(World, Pos, FString::Printf(TEXT("Arm_R : %.01f / %.01f"), Data->Durability, Data->MaxDurability), nullptr, FColor::White, DT);
		}
		
		Data = &Arm_L;
		Pos = ArmL_Pos;
		if (Data->bIsHas)
		{
			DrawDebugString(World, Pos, FString::Printf(TEXT("Arm_L : %.01f / %.01f"), Data->Durability, Data->MaxDurability), nullptr, FColor::White, DT);
		}
		
		Data = &Leg_R;
		Pos = LegR_Pos;
		if (Data->bIsHas)
		{
			DrawDebugString(World, Pos, FString::Printf(TEXT("Leg_R : %.01f / %.01f"), Data->Durability, Data->MaxDurability), nullptr, FColor::White, DT);
		}
		
		Data = &Leg_L;
		Pos = LegL_Pos;
		if (Data->bIsHas)
		{
			DrawDebugString(World, Pos, FString::Printf(TEXT("Leg_L : %.01f / %.01f"), Data->Durability, Data->MaxDurability), nullptr, FColor::White, DT);
		}
		
	}
}

