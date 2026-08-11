#include "AI/Spawning/EnemySpawnSectorVolume.h"

#include "AI/Spawning/ZombieDirectorWorldSubsystem.h"
#include "Components/BoxComponent.h"

AEnemySpawnSectorVolume::AEnemySpawnSectorVolume()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	SectorBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("SectorBounds"));
	SetRootComponent(SectorBounds);
	SectorBounds->SetBoxExtent(FVector(5000.0f, 5000.0f, 2000.0f));
	SectorBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SectorBounds->SetGenerateOverlapEvents(false);
}

void AEnemySpawnSectorVolume::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		if (UZombieDirectorWorldSubsystem* Director =
			GetWorld()->GetSubsystem<UZombieDirectorWorldSubsystem>())
		{
			Director->RegisterSector(this);
		}
	}
}

void AEnemySpawnSectorVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UZombieDirectorWorldSubsystem* Director =
			World->GetSubsystem<UZombieDirectorWorldSubsystem>())
		{
			Director->UnregisterSector(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

bool AEnemySpawnSectorVolume::ContainsLocation(const FVector& WorldLocation) const
{
	if (!IsValid(SectorBounds))
	{
		return false;
	}

	const FVector LocalLocation = SectorBounds->GetComponentTransform().InverseTransformPosition(WorldLocation);
	const FVector Extent = SectorBounds->GetUnscaledBoxExtent();
	return FMath::Abs(LocalLocation.X) <= Extent.X &&
		FMath::Abs(LocalLocation.Y) <= Extent.Y &&
		FMath::Abs(LocalLocation.Z) <= Extent.Z;
}
