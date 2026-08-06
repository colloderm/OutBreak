// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/Components/EnemyPhysicalComponent.h"

#include "AI/EnemyCharacter.h"
#include "AI/Components/EnemyMovementComponent.h"
#include "Engine/StaticMeshActor.h"

#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

// Sets default values for this component's properties
UEnemyPhysicalComponent::UEnemyPhysicalComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	
	Limbes.Add(FName(TEXT("Head")), FLimbData(20, 20));
	Limbes.Add(FName(TEXT("upperarm_r")), FLimbData(40, 40));
	Limbes.Add(FName(TEXT("upperarm_l")), FLimbData(40, 40));
	Limbes.Add(FName(TEXT("thigh_r")), FLimbData(40, 40));
	Limbes.Add(FName(TEXT("thigh_l")), FLimbData(40, 40));
	// ...
}


// Called when the game starts
void UEnemyPhysicalComponent::BeginPlay()
{
	Super::BeginPlay();
	
	
	
	TargetMesh = GetEnemyCharacter()->GetMesh();
	ProxyMesh = GetEnemyCharacter()->GetChildActorSkeletalMesh();
	
	const auto PhysicalReact = EnemyAsset->GetPhysicalReact();
	
	if (IsValid(PhysicalReact->ReactCurveFloat))
	{
		FOnTimelineFloat UpdateDelegate;
		UpdateDelegate.BindUFunction(
			this,
			FName(TEXT("HandleReactTimeline")));
		
		
		FOnTimelineEvent FinishedDelegate;
		FinishedDelegate.BindUFunction(
			this,
			FName(TEXT("HandleReactTimelineFinished")));
		
		ReactTimeline.AddInterpFloat(PhysicalReact->ReactCurveFloat, UpdateDelegate);
		ReactTimeline.SetTimelineFinishedFunc(FinishedDelegate);
		
		ReactTimeline.SetLooping(false);
		ReactTimeline.SetPlayRate(1.f);
	}
	
	if (!ensureAlwaysMsgf(
		PhysicalReact->PM_Head &&
		PhysicalReact->PM_Torso &&
		PhysicalReact->PM_Arm_R &&
		PhysicalReact->PM_Arm_L &&
		PhysicalReact->PM_Leg_R &&
		PhysicalReact->PM_Leg_L,
		TEXT("%s::%s: VaultMontage is invalid."),
		*GetClass()->GetName(),
		TEXT(__FUNCTION__)))
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s: Physical Material is not set."), *GetClass()->GetName(), TEXT(__FUNCTION__));
	}

	// ...
	
}


// Called every frame
void UEnemyPhysicalComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                                FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ReactTimeline.TickTimeline(DeltaTime);
	DrawDebugLimb();
	// ...
}

void UEnemyPhysicalComponent::ApplyDamage(const float DamageAmount)
{
	AEnemyCharacter* OwnerCharacter = GetEnemyCharacter();
	if (DamageAmount <= 0.0f ||
		!IsValid(OwnerCharacter) ||
		OwnerCharacter->IsDead())
	{
		return;
	}

	Health = FMath::Max(0.0f, Health - DamageAmount);
	if (Health <= 0.0f)
	{
		Action_Dead();
	}
}

void UEnemyPhysicalComponent::ActionPhysical(const FHitResult& HitResult, const float DamageAmount)
{
	AEnemyCharacter* OwnerCharacter = GetEnemyCharacter();
	if (DamageAmount <= 0.0f ||
		!IsValid(OwnerCharacter) ||
		OwnerCharacter->IsDead())
	{
		return;
	}
	
	BloodVFX(HitResult);

	const auto PhysicalReact = EnemyAsset->GetPhysicalReact();
	const auto LimbMeshes = EnemyAsset->GetLimbMeshes();
	const float BlendWeight = PhysicalReact->BlendWeight_Anim_Physics;
	
	TWeakObjectPtr<UPhysicalMaterial> PhyMtrl = HitResult.PhysMaterial;
	
	const FName BoneName = HitResult.BoneName;
	const FVector HitDirection = HitResult.Normal;
	
	
	ApplyDamage(DamageAmount);
	if (OwnerCharacter->IsDead())
	{
		return;
	}

	// Pelvis hits still reduce overall health. Only the localized limb and
	// temporary physics reaction are skipped for the root bone.
	if (BoneName == FName(TEXT("pelvis")))
	{
		UE_LOG(
			LogTemp,
			VeryVerbose,
			TEXT("%s::%s: Pelvis hit applied damage; localized reaction skipped."),
			*GetClass()->GetName(),
			TEXT(__FUNCTION__));
		return;
	}
	
	CacheBoneName = BoneName;
	
	OwnerCharacter->StopCharacterMovement();
	
	
	if (PhyMtrl == PhysicalReact->PM_Head)
	{
		ActionLimb(nullptr, FName(TEXT("Head")), DamageAmount);
	}
	else if (PhyMtrl == PhysicalReact->PM_Arm_R)
	{
		ActionLimb(LimbMeshes->SM_Arm_R, FName(TEXT("upperarm_r")), DamageAmount);
	}
	else if (PhyMtrl == PhysicalReact->PM_Arm_L)
	{
		ActionLimb(LimbMeshes->SM_Arm_L, FName(TEXT("upperarm_l")), DamageAmount);
	}
	else if (PhyMtrl == PhysicalReact->PM_Leg_R)
	{
		ActionLimb(LimbMeshes->SM_Leg_R, FName(TEXT("thigh_r")), DamageAmount);
	}
	else if (PhyMtrl == PhysicalReact->PM_Leg_L)
	{
		ActionLimb(LimbMeshes->SM_Leg_L, FName(TEXT("thigh_l")), DamageAmount);
	}

	// Head destruction or a lethal limb combination can enter the dead state
	// inside ActionLimb. Do not restart the temporary hit-reaction timeline,
	// because its finished callback disables skeletal simulation.
	if (OwnerCharacter->IsDead())
	{
		return;
	}
	
	
	if (bIsHit)
	{
		UE_LOG(LogTemp, Display, TEXT("%s::%s: It's already a hit."), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return;
	}
	
	TargetMesh->SetAllBodiesBelowPhysicsBlendWeight(BoneName, BlendWeight);
	TargetMesh->SetAllBodiesBelowSimulatePhysics(BoneName, true, true);
	TargetMesh->AddImpulse(HitDirection.GetSafeNormal() * PhysicalReact->ReactScale, BoneName, true);
	
	ReactTimeline.PlayFromStart();
	
	bIsHit = true;
}

void UEnemyPhysicalComponent::BloodVFX(const FHitResult& HitResult)
{
	if (!IsValid(EnemyAsset))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("%s::%s: EnemyAsset is invalid."),
			*GetClass()->GetName(),
			TEXT(__FUNCTION__)
		);
		return;
	}

	const FEnemyPhysicalReact* PhysicalReact = EnemyAsset->GetPhysicalReact();

	UNiagaraSystem* BulletHit                  = PhysicalReact->Blood_BulletHit;
	UNiagaraSystem* BloodSplatter              = PhysicalReact->Blood_Splatter;
	UNiagaraSystem* BloodSplatterDirection     = PhysicalReact->Blood_Splatter_Direction;

	const FVector SpawnLocation =
		HitResult.ImpactPoint + HitResult.ImpactNormal * 2.0f;

	const FVector SplatterPosition =
		SpawnLocation + FVector(10.010032f, 21.110564f, 15.012697f);

	const FVector DirectionSplatterPosition =
		SpawnLocation + FVector(4.536247f, -42.886109f, 15.012695f);

	auto SpawnConfiguredNiagara =
		[this](
			UNiagaraSystem* System,
			const FVector& Location,
			const FRotator& Rotation,
			const int32 SpawnCount,
			const float LifetimeMultiplier,
			const float ScaleMultiplier
		) -> UNiagaraComponent*
	{
		if (!IsValid(System))
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("%s::%s: Niagara System is invalid."),
				*GetClass()->GetName(),
				TEXT(__FUNCTION__)
			);
			return nullptr;
		}

		UNiagaraComponent* Component =
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				GetWorld(),
				System,
				Location,
				Rotation,
				FVector::OneVector,

				/* bAutoDestroy   */ true,
				/* bAutoActivate  */ false,
				/* PoolingMethod  */ ENCPoolMethod::None,
				/* bPreCullCheck  */ false
			);

		if (!IsValid(Component))
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("%s::%s: Failed to spawn Niagara component: %s"),
				*GetClass()->GetName(),
				TEXT(__FUNCTION__),
				*GetNameSafe(System)
			);
			return nullptr;
		}

		/*
		 * Niagara System에서 아래 값들이
		 * User Parameter로 노출되어 있어야 합니다.
		 *
		 * User.SpawnCount          : int32
		 * User.LifetimeMultiplier  : float
		 * User.ScaleMultiplier     : float
		 */
		Component->SetVariableInt(
			TEXT("User.SpawnCount"),
			SpawnCount
		);

		Component->SetVariableFloat(
			TEXT("User.LifetimeMultiplier"),
			LifetimeMultiplier
		);

		Component->SetVariableFloat(
			TEXT("User.ScaleMultiplier"),
			ScaleMultiplier
		);

		Component->Activate(true);

		return Component;
	};

	SpawnConfiguredNiagara(
		BulletHit,
		SpawnLocation,
		FRotator(0.0f, 90.0f, 0.0f),
		8,
		1.0f,
		2.0f
	);

	SpawnConfiguredNiagara(
		BloodSplatter,
		SplatterPosition,
		FRotator(0.0f, 90.0f, 0.0f),
		4,
		1.0f,
		1.0f
	);

	SpawnConfiguredNiagara(
		BloodSplatterDirection,
		DirectionSplatterPosition,
		FRotator(0.0f, -90.0f, 0.0f),
		2,
		1.0f,
		1.0f
	);
}

void UEnemyPhysicalComponent::ActionLimb(UStaticMesh* MeshAsset, FName BoneName, float DamageAmount)
{
	if (BoneName == NAME_None)
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s: BoneName is Name_None."), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return;
	}
	
	FLimbData* Data = Limbes.Find(BoneName);
	if (Data == nullptr)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("%s::%s: Limb data was not found for bone %s."),
			*GetClass()->GetName(),
			TEXT(__FUNCTION__),
			*BoneName.ToString());
		return;
	}

	if ((Data->bIsHas))
	{
		Data->Durability -= DamageAmount;
		Data->Durability = FMath::Clamp(Data->Durability, 0.f, Data->MaxDurability);
	
		if (Data->Durability <= 0.f)
		{
			Data->bIsHas = false;
			if (IsValid(MeshAsset))
			{
				UWorld* World = GetWorld();

				if (!IsValid(World))
				{
					UE_LOG(LogTemp, Error, TEXT("%s::%s: World is invalid."), *GetClass()->GetName(), TEXT(__FUNCTION__));
					return;
				}
			
				FActorSpawnParameters SpawnParams;
				SpawnParams.Owner = GetOwner();
				SpawnParams.SpawnCollisionHandlingOverride =
					ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

				FTransform SpawnTransform = ProxyMesh->GetSocketTransform(BoneName);
				AStaticMeshActor* MeshPart = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), SpawnTransform, SpawnParams);
				MeshPart->SetLifeSpan(10.f);
				UStaticMeshComponent* MeshComp = Cast<UStaticMeshComponent>(MeshPart->GetRootComponent());
				MeshComp->SetMobility(EComponentMobility::Movable);
				MeshComp->SetStaticMesh(MeshAsset);

				MeshComp->SetCollisionProfileName(TEXT("PhysicsActor"));
				MeshComp->SetGenerateOverlapEvents(false);
				MeshComp->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);

				MeshComp->SetCanEverAffectNavigation(false);
				MeshComp->SetMassOverrideInKg(NAME_None, 300.f);
				MeshComp->SetSimulatePhysics(true);
				MeshComp->WakeAllRigidBodies();
			}
			
			UAnimInstance* AnimInstance = TargetMesh->GetAnimInstance();
			if (IsValid(AnimInstance))
			{
				if (AnimInstance->IsAnyMontagePlaying())
				{
					AnimInstance->StopAllMontages(0.f);
				}
			}
			
			ProxyMesh->HideBoneByName(BoneName, PBO_Term);
			
			AEnemyCharacter* OwnerCharacter = GetEnemyCharacter();
			if (!IsValid(OwnerCharacter))
			{
				UE_LOG(LogTemp, Error, TEXT("%s::%s: Owner Character is invalid."), *GetClass()->GetName(), TEXT(__FUNCTION__));
				return;
			}
			UEnemyMovementComponent* MovementComponent =  Cast<UEnemyMovementComponent>(OwnerCharacter->GetMovementComponent());
			if (!IsValid(MovementComponent))
			{
				UE_LOG(LogTemp, Error, TEXT("%s::%s: Movement Component is invalid."), *GetClass()->GetName(), TEXT(__FUNCTION__));
				return;
			}
			
			
			MovementComponent->SetLocomotationState(EvaluateLocomotionState());
		}
	}
	
	const FLimbData* HeadData = Limbes.Find(FName(TEXT("Head")));
	if (HeadData != nullptr && !HeadData->bIsHas)
	{
		Action_Dead();
	}
}

ELocomotionWalkRunState UEnemyPhysicalComponent::EvaluateLocomotionState() const
{
	const FLimbData* Arm_R = Limbes.Find(FName(TEXT("upperarm_r")));
	const FLimbData* Arm_L = Limbes.Find(FName(TEXT("upperarm_l")));
	const FLimbData* Leg_R = Limbes.Find(FName(TEXT("thigh_r")));
	const FLimbData* Leg_L = Limbes.Find(FName(TEXT("thigh_l")));
	
	
	int MissingLimb_Arm = FMath::Clamp((!Arm_R->bIsHas + !Arm_L->bIsHas), 0, 2);
	
	
	if (!Leg_R->bIsHas || !Leg_L->bIsHas)
	{
		if (MissingLimb_Arm == 2)
		{
			return ELocomotionWalkRunState::Dead;	
		}
		else if (MissingLimb_Arm == 1)
		{
			return ELocomotionWalkRunState::SlowCrawling;
		}
		
		return ELocomotionWalkRunState::Crawling;
	}
	return ELocomotionWalkRunState::Walking;
}

EEnemyMissingArmState UEnemyPhysicalComponent::GetMissingArmState() const
{
	const FLimbData* RightArm =
		Limbes.Find(FName(TEXT("upperarm_r")));
	const FLimbData* LeftArm =
		Limbes.Find(FName(TEXT("upperarm_l")));

	const bool bRightArmMissing =
		RightArm == nullptr || !RightArm->bIsHas;
	const bool bLeftArmMissing =
		LeftArm == nullptr || !LeftArm->bIsHas;

	if (bLeftArmMissing && bRightArmMissing)
	{
		return EEnemyMissingArmState::Both;
	}

	if (bLeftArmMissing)
	{
		return EEnemyMissingArmState::Left;
	}

	if (bRightArmMissing)
	{
		return EEnemyMissingArmState::Right;
	}

	return EEnemyMissingArmState::None;
}

void UEnemyPhysicalComponent::Action_Dead()
{
	Health = 0.0f;
	ReactTimeline.Stop();
	if (AEnemyCharacter* OwnerCharacter = GetEnemyCharacter())
	{
		OwnerCharacter->Dead();
	}
}

void UEnemyPhysicalComponent::DrawDebugLimb()
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
		
		FLimbData* Data = Limbes.Find(FName(TEXT("Head")));
		FVector Pos = Head_Pos;
		if (Data->bIsHas)
		{
			DrawDebugString(World, Pos, FString::Printf(TEXT("Head : %.01f / %.01f"), Data->Durability, Data->MaxDurability), nullptr, FColor::White, DT);
		}
		
		
		Data = Limbes.Find(FName(TEXT("upperarm_r")));
		Pos = ArmR_Pos;
		if (Data->bIsHas)
		{
			DrawDebugString(World, Pos, FString::Printf(TEXT("Arm_R : %.01f / %.01f"), Data->Durability, Data->MaxDurability), nullptr, FColor::White, DT);
		}
		
		Data = Limbes.Find(FName(TEXT("upperarm_l")));
		Pos = ArmL_Pos;
		if (Data->bIsHas)
		{
			DrawDebugString(World, Pos, FString::Printf(TEXT("Arm_L : %.01f / %.01f"), Data->Durability, Data->MaxDurability), nullptr, FColor::White, DT);
		}
		
		Data = Limbes.Find(FName(TEXT("thigh_r")));
		Pos = LegR_Pos;
		if (Data->bIsHas)
		{
			DrawDebugString(World, Pos, FString::Printf(TEXT("Leg_R : %.01f / %.01f"), Data->Durability, Data->MaxDurability), nullptr, FColor::White, DT);
		}
		
		Data = Limbes.Find(FName(TEXT("thigh_l")));
		Pos = LegL_Pos;
		if (Data->bIsHas)
		{
			DrawDebugString(World, Pos, FString::Printf(TEXT("Leg_L : %.01f / %.01f"), Data->Durability, Data->MaxDurability), nullptr, FColor::White, DT);
		}
		
	}

}

void UEnemyPhysicalComponent::HandleReactTimeline(float value)
{
	if (CacheBoneName == NAME_None)
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s: Cache Bone Name is \"NAME_None\""), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return;
	}
	TargetMesh->SetAllBodiesBelowPhysicsBlendWeight(CacheBoneName, value);
}

void UEnemyPhysicalComponent::HandleReactTimelineFinished()
{
	TargetMesh->SetAllBodiesPhysicsBlendWeight(0.0f, false);
	TargetMesh->SetAllBodiesSimulatePhysics(false);
	bIsHit = false;
}

