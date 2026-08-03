#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMeshActor.h"
#include "InteriorPCGStaticMeshActor.generated.h"

UCLASS(BlueprintType)
class INTERIORPCGRUNTIME_API AInteriorPCGStaticMeshActor : public AStaticMeshActor
{
	GENERATED_BODY()

public:
	AInteriorPCGStaticMeshActor(const FObjectInitializer& ObjectInitializer);
};
