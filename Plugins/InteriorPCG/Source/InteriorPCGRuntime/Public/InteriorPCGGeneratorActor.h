// Copyright OutBreak. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InteriorPCGCoreTypes.h"
#include "InteriorPCGGeneratorActor.generated.h"

class UActorComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UInteriorPCGGenerationProfile;
class USceneComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInteriorPCGGeneratedSignature, const FInteriorPCGGenerationResult&, Result);

/**
 * Optional direct renderer for the semantic solver. Repeated static meshes use HISM; interactive variants spawn actors.
 * The same profile can instead be consumed by the native PCG graph node when a graph-driven output is preferred.
 */
UCLASS(Blueprintable, ClassGroup = "Procedural")
class INTERIORPCGRUNTIME_API AInteriorPCGGeneratorActor : public AActor
{
	GENERATED_BODY()

public:
	AInteriorPCGGeneratorActor();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginDestroy() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interior PCG")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interior PCG")
	TObjectPtr<UInteriorPCGGenerationProfile> Profile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior PCG")
	FInteriorPCGGenerationOptions GenerationOptions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior PCG|Output")
	bool bGenerateOnConstruction = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior PCG|Output")
	bool bSpawnInteractiveActors = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interior PCG|Output")
	bool bEnableCollision = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Interior PCG|Output")
	FInteriorPCGGenerationResult LastResult;

	UPROPERTY(BlueprintAssignable, Category = "Interior PCG")
	FInteriorPCGGeneratedSignature OnGenerated;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Interior PCG")
	bool Generate();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Interior PCG")
	void ClearGenerated();

	UFUNCTION(BlueprintPure, Category = "Interior PCG")
	bool Validate(TArray<FString>& OutMessages) const;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UActorComponent>> GeneratedComponents;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> GeneratedActors;

	bool bIsGenerating = false;

	void BuildOutputComponents();
};
