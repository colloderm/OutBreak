#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Game/Expedition/OBHelicopterTypes.h"
#include "OBLandingZoneScannerComponent.generated.h"

/** Server-side landing-zone validator used by insertion and extraction fallbacks. */
UCLASS(ClassGroup = (OutBreak), meta = (BlueprintSpawnableComponent))
class OUTBREAK_API UOBLandingZoneScannerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOBLandingZoneScannerComponent();

	UFUNCTION(BlueprintCallable, Category = "Helicopter|Landing Zone")
	bool FindSafeLandingZone(const FVector& RequestedLocation, FOBLandingZoneResult& OutResult) const;

	UFUNCTION(BlueprintCallable, Category = "Helicopter|Landing Zone")
	bool FindGroundAtXY(const FVector2D& WorldXY, FVector& OutGroundLocation) const;

protected:
	bool EvaluateCandidate(const FVector& RequestedLocation, const FVector2D& CandidateXY, FOBLandingZoneResult& OutResult) const;
	bool IsInsideExclusionVolume(const FVector& Location) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landing Zone|Search", meta = (ClampMin = "0"))
	float SearchRadius = 6000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landing Zone|Search", meta = (ClampMin = "100"))
	float RingStep = 1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landing Zone|Search", meta = (ClampMin = "4", ClampMax = "32"))
	int32 SamplesPerRing = 12;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landing Zone|Ground", meta = (ClampMin = "1000"))
	float TraceHalfHeight = 100000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landing Zone|Ground", meta = (ClampMin = "0", ClampMax = "45"))
	float MaxSlopeDegrees = 18.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landing Zone|Ground", meta = (ClampMin = "100"))
	float FootprintRadius = 450.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landing Zone|Ground", meta = (ClampMin = "0"))
	float MaxHeightVariance = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landing Zone|Navigation")
	bool bRequireNavigation = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landing Zone|Navigation")
	FVector NavigationProjectionExtent = FVector(500.f, 500.f, 500.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landing Zone|Helicopter", meta = (ClampMin = "100"))
	float HoverHeight = 1800.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landing Zone|Helicopter", meta = (ClampMin = "100"))
	float HelicopterClearanceRadius = 900.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landing Zone|Helicopter", meta = (ClampMin = "100"))
	float HelicopterClearanceHalfHeight = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landing Zone|Collision")
	TEnumAsByte<ECollisionChannel> GroundTraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landing Zone|Debug")
	bool bDrawDebug = false;
};
