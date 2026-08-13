#include "Weapon/OBExplosionLibrary.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Ability/Tags/OBGameplayTags.h"
#include "AI/Components/EnemyPhysicalComponent.h"
#include "AI/EnemyCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"

void UOBExplosionLibrary::ApplyExplosion(
	const UObject* WorldContextObject,
	const FVector Center,
	const float Radius,
	const float BaseDamage,
	TSubclassOf<UGameplayEffect> DamageEffect,
	AController* InstigatorController,
	AActor* DamageCauser,
	const float ImpulseStrength,
	const TArray<AActor*>& IgnoreActors,
	UAbilitySystemComponent* SourceASCOverride)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;

	// 판정은 서버 전용. 클라가 부르면 이중 피해가 난다.
	if (!World || World->GetNetMode() == NM_Client || Radius <= 0.f) return;

	FCollisionQueryParams Params;
	Params.AddIgnoredActors(IgnoreActors);
	if (DamageCauser)
	{
		Params.AddIgnoredActor(DamageCauser);
	}

	// 채널 응답 설정에 좌우되지 않도록 오브젝트 타입으로 찾는다.
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(Radius), Params);

	UAbilitySystemComponent* SourceASC = SourceASCOverride;
	if (!SourceASC && InstigatorController)
	{
		SourceASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(InstigatorController->GetPawn());
	}

	TSet<AActor*> AlreadyHit;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		// continue다. return이면 나머지 대상이 통째로 날아간다.
		if (!Actor || AlreadyHit.Contains(Actor)) continue;
		AlreadyHit.Add(Actor);

		// 제곱근 감쇠. 선형이면 반경 끝에서 계수가 0에 수렴해 무력해진다.
		const float Dist = FVector::Dist(Center, Actor->GetActorLocation());
		const float Falloff = FMath::Sqrt(FMath::Clamp(1.f - (Dist / Radius), 0.f, 1.f));
		const float FinalDamage = BaseDamage * Falloff;
		const FVector FromCenter = (Actor->GetActorLocation() - Center).GetSafeNormal();

		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor);

		if (FinalDamage > 0.f)
		{
			if (TargetASC)
			{
				// 플레이어. GE가 없으면 아무 일도 안 일어나므로 조용히 넘기지 않는다.
				UAbilitySystemComponent* EffectSource = SourceASC ? SourceASC : TargetASC;
				if (!DamageEffect)
				{
					UE_LOG(LogTemp, Warning,
						TEXT("[Explosion] DamageEffect가 비어 있어 %s 는 피해를 받지 않는다. Causer=%s"),
						*Actor->GetName(), *GetNameSafe(DamageCauser));
				}
				else
				{
					FGameplayEffectContextHandle Ctx = EffectSource->MakeEffectContext();
					Ctx.AddInstigator(InstigatorController ? InstigatorController->GetPawn() : nullptr, DamageCauser);

					FGameplayEffectSpecHandle Spec = EffectSource->MakeOutgoingSpec(DamageEffect, 1.f, Ctx);
					if (Spec.IsValid())
					{
						Spec.Data->SetSetByCallerMagnitude(OBGameplayTags::SetByCaller_Damage, FinalDamage);
						EffectSource->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
					}
				}
			}
			else
			{
				// 좀비는 ASC가 없다. TakeDamage → EnemyPhysicalComponent 경로로 보낸다.
				UGameplayStatics::ApplyDamage(Actor, FinalDamage, InstigatorController, DamageCauser, UDamageType::StaticClass());

				// 총격과 동일한 피격 연출. 폭심 쪽 표면에서 피가 튀도록 방향을 잡는다.
				if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(Actor))
				{
					if (UEnemyPhysicalComponent* Physical = Enemy->FindComponentByClass<UEnemyPhysicalComponent>())
					{
						const FVector ToCenter = -FromCenter;
						// 발밑이 아니라 몸통 높이에서 터지도록 살짝 올린다.
						const FVector ImpactPoint = Actor->GetActorLocation() + ToCenter * 40.f + FVector(0.f, 0.f, 60.f);
						Physical->Multicast_BloodVFX(ImpactPoint, ToCenter);
					}
				}
			}
		}

		// 임펄스는 피해 뒤에 준다. 그래야 방금 죽어 래그돌이 된 좀비도 날아간다.
		if (ImpulseStrength > 0.f)
		{
			const float Impulse = ImpulseStrength * Falloff;
			ACharacter* Character = Cast<ACharacter>(Actor);
			USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;

			if (Mesh && Mesh->IsSimulatingPhysics())
			{
				Mesh->AddRadialImpulse(Center, Radius, Impulse, RIF_Linear, /*bVelChange=*/true);
			}
			else if (Character)
			{
				// 살아있는 캐릭터는 캡슐이 물리를 안 쓴다. LaunchCharacter가 유일한 경로다.
				// 위로 살짝 섞어야 지면 마찰에 먹히지 않고 날아간다.
				const FVector Launch = (FromCenter + FVector(0.f, 0.f, 0.5f)).GetSafeNormal() * Impulse;
				Character->LaunchCharacter(Launch, true, true);
			}
			else if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Actor->GetRootComponent()))
			{
				if (Prim->IsSimulatingPhysics())
				{
					Prim->AddRadialImpulse(Center, Radius, Impulse, RIF_Linear, true);
				}
			}
		}
	}
}