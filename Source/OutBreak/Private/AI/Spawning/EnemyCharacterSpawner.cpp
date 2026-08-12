#include "AI/Spawning/EnemyCharacterSpawner.h"

#include "AI/EnemyCharacter.h"
#include "AI/Spawning/EnemyDirectorSettings.h"
#include "AI/Spawning/EnemySpawnProfile.h"
#include "AI/Spawning/EnemySpawnTypes.h"
#include "AI/Spawning/ZombieDirectorWorldSubsystem.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "NavigationSystem.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

AEnemyCharacterSpawner::AEnemyCharacterSpawner()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	SpawnDirection = CreateDefaultSubobject<UArrowComponent>(TEXT("SpawnDirection"));
	SpawnDirection->SetupAttachment(SceneRoot);

	OccupancyPreview = CreateDefaultSubobject<UBoxComponent>(TEXT("OccupancyPreview"));
	OccupancyPreview->SetupAttachment(SceneRoot);
	OccupancyPreview->SetBoxExtent(FVector(55.0f, 55.0f, 100.0f));
	OccupancyPreview->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	OccupancyPreview->SetGenerateOverlapEvents(false);

}

void AEnemyCharacterSpawner::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AEnemyCharacterSpawner, CurrentSpawnCharges);
}

void AEnemyCharacterSpawner::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		InitializeSpawnCharges();
		if (UZombieDirectorWorldSubsystem* Director =
			GetWorld()->GetSubsystem<UZombieDirectorWorldSubsystem>())
		{
			Director->RegisterSpawner(this);
		}
	}
}

void AEnemyCharacterSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChargeRechargeTimerHandle);
		if (UZombieDirectorWorldSubsystem* Director =
			World->GetSubsystem<UZombieDirectorWorldSubsystem>())
		{
			Director->UnregisterSpawner(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

bool AEnemyCharacterSpawner::TryResolveSafeSpawnTransform(
	FTransform& OutTransform,
	FString* OutFailureReason) const
{
	auto Fail = [OutFailureReason](const TCHAR* Reason)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = Reason;
		}
		return false;
	};

	UWorld* World = GetWorld();
	const TSubclassOf<AEnemyCharacter> EnemyClassToSpawn = ResolveEnemyClass();
	if (!IsValid(World) || !EnemyClassToSpawn)
	{
		return Fail(TEXT("world or enemy class missing"));
	}

	OutTransform = ResolveSpawnTransform();
	const FVector RequestedLocation = OutTransform.GetLocation();
	const AEnemyCharacter* EnemyCDO = EnemyClassToSpawn->GetDefaultObject<AEnemyCharacter>();
	const UCapsuleComponent* Capsule = IsValid(EnemyCDO)
		? EnemyCDO->GetCapsuleComponent()
		: nullptr;

	const FVector SpawnScale = OutTransform.GetScale3D().GetAbs();
	const float RadiusScale = FMath::Max(0.01f, FMath::Max(SpawnScale.X, SpawnScale.Y));
	const float HeightScale = FMath::Max(0.01f, SpawnScale.Z);
	const float CapsuleRadius = Capsule
		? FMath::Max(1.0f, Capsule->GetUnscaledCapsuleRadius() * RadiusScale)
		: 42.0f;
	const float CapsuleHalfHeight = Capsule
		? FMath::Max(CapsuleRadius, Capsule->GetUnscaledCapsuleHalfHeight() * HeightScale)
		: 96.0f;
	const float Clearance = FMath::Max(0.0f, SpawnCapsuleClearance);
	const ECollisionChannel SpawnObjectChannel = Capsule
		? Capsule->GetCollisionObjectType()
		: ECC_Pawn;

	FCollisionObjectQueryParams GroundObjects;
	GroundObjects.AddObjectTypesToQuery(ECC_WorldStatic);
	GroundObjects.AddObjectTypesToQuery(ECC_WorldDynamic);

	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(EnemySpawnerSafePlacement),
		false,
		this);
	const FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(
		CapsuleRadius + Clearance,
		CapsuleHalfHeight + Clearance);
	FCollisionObjectQueryParams StaticMeshCollisionObjects;
	StaticMeshCollisionObjects.AddObjectTypesToQuery(ECC_WorldStatic);
	StaticMeshCollisionObjects.AddObjectTypesToQuery(ECC_WorldDynamic);

	auto CollectBlockingStaticMeshes = [
		World,
		SpawnObjectChannel,
		&StaticMeshCollisionObjects,
		&CapsuleShape,
		&QueryParams,
		&OutTransform](
			const FVector& CandidateLocation,
			TArray<FOverlapResult>* OutStaticMeshOverlaps = nullptr)
	{
		TArray<FOverlapResult> Overlaps;
		World->OverlapMultiByObjectType(
			Overlaps,
			CandidateLocation,
			OutTransform.GetRotation(),
			StaticMeshCollisionObjects,
			CapsuleShape,
			QueryParams);

		bool bBlockedByStaticMesh = false;
		for (const FOverlapResult& Overlap : Overlaps)
		{
			const UStaticMeshComponent* StaticMeshComponent =
				Cast<UStaticMeshComponent>(Overlap.GetComponent());
			if (!IsValid(StaticMeshComponent) ||
				StaticMeshComponent->GetCollisionEnabled() == ECollisionEnabled::NoCollision ||
				StaticMeshComponent->GetCollisionResponseToChannel(SpawnObjectChannel) != ECR_Block)
			{
				continue;
			}

			bBlockedByStaticMesh = true;
			if (OutStaticMeshOverlaps)
			{
				OutStaticMeshOverlaps->Add(Overlap);
			}
		}
		return bBlockedByStaticMesh;
	};

	TArray<FVector2D, TInlineAllocator<25>> SearchOffsets;
	SearchOffsets.Add(FVector2D::ZeroVector);
	const float HorizontalRadius = FMath::Max(0.0f, CollisionSearchRadius);
	if (HorizontalRadius > KINDA_SMALL_NUMBER)
	{
		for (int32 Ring = 1; Ring <= 3; ++Ring)
		{
			const float RingRadius = HorizontalRadius * static_cast<float>(Ring) / 3.0f;
			for (int32 Direction = 0; Direction < 8; ++Direction)
			{
				const float Angle = UE_TWO_PI * static_cast<float>(Direction) / 8.0f;
				SearchOffsets.Add(FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * RingRadius);
			}
		}
	}

	TArray<float, TInlineAllocator<20>> VerticalOffsets;
	VerticalOffsets.Add(0.0f);
	const float MaxVerticalAdjustment = FMath::Max(0.0f, MaxVerticalSpawnAdjustment);
	const float FineSearchHeight = FMath::Min(400.0f, MaxVerticalAdjustment);
	for (float Lift = 50.0f; Lift <= FineSearchHeight; Lift += 50.0f)
	{
		VerticalOffsets.Add(Lift);
	}
	for (float Lift = 600.0f; Lift < MaxVerticalAdjustment; Lift += 200.0f)
	{
		VerticalOffsets.Add(Lift);
	}
	if (MaxVerticalAdjustment > FineSearchHeight)
	{
		VerticalOffsets.AddUnique(MaxVerticalAdjustment);
	}

	UNavigationSystemV1* Navigation = UNavigationSystemV1::GetCurrent(World);
	const FVector NavigationExtent(
		FMath::Max(1.0f, NavigationSearchRadius),
		FMath::Max(1.0f, NavigationSearchRadius),
		FMath::Max(1.0f, NavigationSearchHeight));
	bool bFoundNavigationProjection = false;
	TArray<FVector, TInlineAllocator<25>> CapsuleBaseLocations;

	for (const FVector2D& Offset : SearchOffsets)
	{
		FVector ProbeLocation = RequestedLocation + FVector(Offset.X, Offset.Y, 0.0f);
		FNavLocation ProjectedLocation;
		const bool bProjectedToNavigation = IsValid(Navigation) &&
			Navigation->ProjectPointToNavigation(
				ProbeLocation,
				ProjectedLocation,
				NavigationExtent);
		bFoundNavigationProjection |= bProjectedToNavigation;

		if (bRequireNavigation && !bProjectedToNavigation && !bAllowNavigationFallback)
		{
			continue;
		}

		FVector SurfaceLocation = ProbeLocation;
		if (bProjectedToNavigation)
		{
			SurfaceLocation = ProjectedLocation.Location;
		}
		else
		{
			FHitResult GroundHit;
			const FVector TraceStart = ProbeLocation + FVector(0.0f, 0.0f, 50.0f);
			const FVector TraceEnd = ProbeLocation - FVector(
				0.0f,
				0.0f,
				FMath::Max(100.0f, NavigationSearchHeight));
			if (World->LineTraceSingleByObjectType(
				GroundHit,
				TraceStart,
				TraceEnd,
				GroundObjects,
				QueryParams))
			{
				SurfaceLocation = GroundHit.ImpactPoint;
			}
		}

		CapsuleBaseLocations.Add(FVector(
			SurfaceLocation.X,
			SurfaceLocation.Y,
			SurfaceLocation.Z + CapsuleHalfHeight + Clearance));
	}

	// Prefer a nearby lateral correction before lifting the zombie. This avoids
	// choosing a roof or upper floor when an open spot exists beside the marker.
	for (const float VerticalOffset : VerticalOffsets)
	{
		for (const FVector& CapsuleBaseLocation : CapsuleBaseLocations)
		{
			const FVector CandidateLocation =
				CapsuleBaseLocation + FVector(0.0f, 0.0f, VerticalOffset);
			// Only static-mesh geometry may relocate a spawn. Pawns, skeletal
			// meshes, logical volumes, and other enemies are intentionally ignored
			// so a busy spawner does not drift into an unintended ring location.
			if (!CollectBlockingStaticMeshes(CandidateLocation))
			{
				OutTransform.SetLocation(CandidateLocation);
				return true;
			}
		}
	}

	if (bRequireNavigation && !bFoundNavigationProjection && !bAllowNavigationFallback)
	{
		return Fail(TEXT("navigation projection failed"));
	}

	const FVector DiagnosticBaseLocation = CapsuleBaseLocations.IsEmpty()
		? RequestedLocation + FVector(0.0f, 0.0f, CapsuleHalfHeight + Clearance)
		: CapsuleBaseLocations[0];
	const FVector DiagnosticLocation = DiagnosticBaseLocation +
		FVector(0.0f, 0.0f, MaxVerticalAdjustment);
	TArray<FOverlapResult> DiagnosticOverlaps;
	CollectBlockingStaticMeshes(DiagnosticLocation, &DiagnosticOverlaps);

	FString BlockerSummary;
	for (const FOverlapResult& Overlap : DiagnosticOverlaps)
	{
		const FString BlockerName = GetNameSafe(Overlap.GetActor());
		if (!BlockerName.IsEmpty() && !BlockerSummary.Contains(BlockerName))
		{
			if (!BlockerSummary.IsEmpty())
			{
				BlockerSummary += TEXT(",");
			}
			BlockerSummary += BlockerName;
			if (BlockerSummary.Len() >= 160)
			{
				break;
			}
		}
	}

	if (OutFailureReason)
	{
		*OutFailureReason = FString::Printf(
			TEXT("no static-mesh-free spawn location; Capsule(R=%.1f,H=%.1f) MaxLift=%.1f NavProjected=%s StaticMeshBlockers=%s"),
			CapsuleRadius,
			CapsuleHalfHeight,
			MaxVerticalAdjustment,
			bFoundNavigationProjection ? TEXT("true") : TEXT("false"),
			BlockerSummary.IsEmpty() ? TEXT("none at highest probe") : *BlockerSummary);
	}
	return false;
}

bool AEnemyCharacterSpawner::CanSpawnForNoise(
	const FEnemyNoiseEvent& NoiseEvent,
	FString* OutFailureReason) const
{
	auto Fail = [OutFailureReason](const TCHAR* Reason)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = Reason;
		}
		return false;
	};

	if (!bEnabled || !ResolveEnemyClass())
	{
		return Fail(TEXT("disabled or enemy class missing"));
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return Fail(TEXT("world missing"));
	}

	if (CurrentSpawnCharges <= 0)
	{
		return Fail(*FString::Printf(
			TEXT("spawn charges depleted (0/%d)"),
			ResolveMaxSpawnCharges()));
	}

	if (World->GetTimeSeconds() - LastUsedTime < ReuseCooldown)
	{
		return Fail(TEXT("cooldown"));
	}

	FTransform SpawnTransform;
	FString PlacementFailure;
	if (!TryResolveSafeSpawnTransform(SpawnTransform, &PlacementFailure))
	{
		return Fail(*PlacementFailure);
	}

	const FVector SpawnLocation = SpawnTransform.GetLocation();
	const float EffectiveRange = NoiseEvent.MaxRange > 0.0f
		? NoiseEvent.MaxRange
		: TNumericLimits<float>::Max();
	if (FVector::DistSquared2D(SpawnLocation, NoiseEvent.Location) > FMath::Square(EffectiveRange))
	{
		return Fail(TEXT("outside noise range"));
	}

	if (MinPlayerDistance > 0.0f)
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const APlayerController* PlayerController = It->Get();
			const APawn* Pawn = IsValid(PlayerController) ? PlayerController->GetPawn() : nullptr;
			if (IsValid(Pawn) &&
				FVector::DistSquared(SpawnLocation, Pawn->GetActorLocation()) < FMath::Square(MinPlayerDistance))
			{
				return Fail(TEXT("too close to player"));
			}
		}
	}

	return true;
}

FTransform AEnemyCharacterSpawner::ResolveSpawnTransform() const
{
	FTransform Result = SpawnDirection->GetComponentTransform();
	if (IsValid(SpawnProfile))
	{
		Result.AddToTranslation(FVector(0.0f, 0.0f, SpawnProfile->GroundOffset));
	}
	return Result;
}

TSubclassOf<AEnemyCharacter> AEnemyCharacterSpawner::ResolveEnemyClass() const
{
	return IsValid(SpawnProfile) && SpawnProfile->EnemyClass
		? SpawnProfile->EnemyClass
		: EnemyClass;
}

FName AEnemyCharacterSpawner::ResolvePoolKey() const
{
	return IsValid(SpawnProfile) && !SpawnProfile->PoolKey.IsNone()
		? SpawnProfile->PoolKey
		: PoolKey;
}

UAnimMontage* AEnemyCharacterSpawner::ResolveSpawnMontage() const
{
	return IsValid(SpawnProfile) ? SpawnProfile->SpawnMontage.Get() : SpawnMontage.Get();
}

float AEnemyCharacterSpawner::ResolvePresentationDuration() const
{
	return IsValid(SpawnProfile)
		? SpawnProfile->PresentationDuration
		: PresentationDuration;
}

int32 AEnemyCharacterSpawner::ResolveWarmPoolCount() const
{
	return IsValid(SpawnProfile) ? SpawnProfile->WarmPoolCount : WarmPoolCount;
}

int32 AEnemyCharacterSpawner::ResolveInitialSpawnCharges() const
{
	const UEnemyDirectorSettings* Settings = GetDefault<UEnemyDirectorSettings>();
	return FMath::Clamp(
		Settings->DefaultSpawnerInitialCharges + InitialChargeBonus,
		0,
		ResolveMaxSpawnCharges());
}

int32 AEnemyCharacterSpawner::ResolveMaxSpawnCharges() const
{
	return FMath::Max(
		1,
		GetDefault<UEnemyDirectorSettings>()->DefaultSpawnerMaxCharges + MaxChargeBonus);
}

int32 AEnemyCharacterSpawner::ResolveRechargeAmount() const
{
	return FMath::Max(
		0,
		GetDefault<UEnemyDirectorSettings>()->DefaultSpawnerRechargeAmount + RechargeAmountBonus);
}

float AEnemyCharacterSpawner::ResolveRechargeInterval() const
{
	return FMath::Max(
		0.1f,
		GetDefault<UEnemyDirectorSettings>()->DefaultSpawnerRechargeInterval +
			RechargeIntervalAdjustment);
}

void AEnemyCharacterSpawner::MarkUsed()
{
	LastUsedTime = IsValid(GetWorld()) ? GetWorld()->GetTimeSeconds() : 0.0;
	if (HasAuthority())
	{
		CurrentSpawnCharges = FMath::Max(0, CurrentSpawnCharges - 1);
		ForceNetUpdate();
		UE_LOG(
			LogZombieDirector,
			Log,
			TEXT("Spawner %s consumed one charge. Charges=%d/%d"),
			*GetNameSafe(this),
			CurrentSpawnCharges,
			ResolveMaxSpawnCharges());
	}
}

void AEnemyCharacterSpawner::InitializeSpawnCharges()
{
	if (!HasAuthority() || !IsValid(GetWorld()))
	{
		return;
	}

	CurrentSpawnCharges = ResolveInitialSpawnCharges();
	GetWorld()->GetTimerManager().ClearTimer(ChargeRechargeTimerHandle);
	if (ResolveRechargeAmount() > 0)
	{
		GetWorld()->GetTimerManager().SetTimer(
			ChargeRechargeTimerHandle,
			this,
			&AEnemyCharacterSpawner::RechargeSpawnCharges,
			ResolveRechargeInterval(),
			true);
	}
	ForceNetUpdate();
}

void AEnemyCharacterSpawner::RechargeSpawnCharges()
{
	if (!HasAuthority())
	{
		return;
	}

	const int32 PreviousCharges = CurrentSpawnCharges;
	CurrentSpawnCharges = FMath::Min(
		ResolveMaxSpawnCharges(),
		CurrentSpawnCharges + ResolveRechargeAmount());
	if (CurrentSpawnCharges != PreviousCharges)
	{
		ForceNetUpdate();
		UE_LOG(
			LogZombieDirector,
			Log,
			TEXT("Spawner %s recharged %d charge(s). Charges=%d/%d NextInterval=%.1fs"),
			*GetNameSafe(this),
			CurrentSpawnCharges - PreviousCharges,
			CurrentSpawnCharges,
			ResolveMaxSpawnCharges(),
			ResolveRechargeInterval());
	}
}
