// Copyright OutBreak. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "InteriorPCGCoreTypes.h"
#include "InteriorPCGGenerationLibrary.generated.h"

class UInteriorPCGGenerationProfile;

/** Stateless, deterministic rule solver shared by the actor and the native PCG graph node. */
UCLASS()
class INTERIORPCGRUNTIME_API UInteriorPCGGenerationLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Interior PCG")
	static bool Generate(const UInteriorPCGGenerationProfile* Profile, const FInteriorPCGGenerationOptions& Options,
		FInteriorPCGGenerationResult& OutResult);

	UFUNCTION(BlueprintPure, Category = "Interior PCG")
	static bool ValidateProfile(const UInteriorPCGGenerationProfile* Profile, TArray<FString>& OutErrors);

	/** Seed derivation is exposed so downstream PCG graphs can reproduce floor/room/detail choices. */
	UFUNCTION(BlueprintPure, Category = "Interior PCG")
	static FInteriorPCGSeedBundle MakeSeedBundle(int32 BuildingSeed, int32 FloorIndex, int32 RoomID);
};
