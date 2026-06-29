// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FlowFieldDensityComponent.generated.h"

UCLASS(ClassGroup = (FlowField), meta = (BlueprintSpawnableComponent))
class OUTBREAK_API UFlowFieldDensityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFlowFieldDensityComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Enables non-physical, density-based Pawn-to-Pawn separation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|Separation")
	bool bEnableDensitySeparation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|Separation", meta = (ClampMin = "0.0"))
	float SeparationRadius = 140.0f;

	/** Acceleration applied at full overlap density. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|Separation", meta = (ClampMin = "0.0"))
	float SeparationStrength = 250.0f;

	/** Maximum XY velocity contributed by density separation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|Separation", meta = (ClampMin = "0.0"))
	float MaxSeparationVelocity = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|Separation", meta = (ClampMin = "0.0"))
	float SeparationVelocityDamping = 12.0f;

	/** Predicts nearby Pawn convergence before capsules visually overlap. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|Separation", meta = (ClampMin = "0.0"))
	float SeparationPredictionTime = 0.35f;

	/** Larger values keep distant Pawns calm while strengthening the response near contact. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|Separation", meta = (ClampMin = "1.0"))
	float SeparationDensityExponent = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|Separation|Debug")
	bool bDrawDensitySeparationDebug = false;

	bool IsDensitySeparationEnabled() const;
	FVector GetSeparationVelocity() const;
	void ApplySeparationVelocityDelta(const FVector& VelocityDelta);
	void ResetSeparationVelocity();

private:
	void DecaySeparationVelocity(float DeltaTime);
	void DrawDensitySeparationDebug() const;

	FVector SeparationVelocity = FVector::ZeroVector;
};
