#include "InteriorPCGStaticMeshActor.h"

#include "Components/StaticMeshComponent.h"

AInteriorPCGStaticMeshActor::AInteriorPCGStaticMeshActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetMobility(EComponentMobility::Movable);
	GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GetStaticMeshComponent()->SetCollisionResponseToAllChannels(ECR_Block);
}
