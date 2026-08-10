#pragma once

#include "CoreMinimal.h"
#include "OBHelicopterTypes.generated.h"

class AOBInsertionHelicopter;

UENUM(BlueprintType)
enum class EOBHelicopterMission : uint8
{
	None       UMETA(DisplayName = "None"),
	Insertion  UMETA(DisplayName = "Insertion"),
	Extraction UMETA(DisplayName = "Extraction")
};

UENUM(BlueprintType)
enum class EOBHelicopterRoutePurpose : uint8
{
	InsertionOrbit     UMETA(DisplayName = "Insertion Orbit"),
	ExtractionApproach UMETA(DisplayName = "Extraction Approach"),
	ExtractionExit     UMETA(DisplayName = "Extraction Exit")
};

UENUM(BlueprintType)
enum class EOBInsertionPhase : uint8
{
	None             UMETA(DisplayName = "None"),
	WaitingForTarget UMETA(DisplayName = "Waiting For Target"),
	Orbiting         UMETA(DisplayName = "Orbiting"),
	Approaching      UMETA(DisplayName = "Approaching"),
	Scanning         UMETA(DisplayName = "Scanning"),
	Hovering         UMETA(DisplayName = "Hovering"),
	Rappelling       UMETA(DisplayName = "Rappelling"),
	Departing        UMETA(DisplayName = "Departing"),
	Completed        UMETA(DisplayName = "Completed"),
	Aborted          UMETA(DisplayName = "Aborted"),
	// Appended to preserve the serialized values of the original phases.
	LoadingTarget    UMETA(DisplayName = "Loading Target"),
	ValidatingTarget UMETA(DisplayName = "Validating Target")
};

UENUM(BlueprintType)
enum class EOBLandingZoneFailure : uint8
{
	None                       UMETA(DisplayName = "None"),
	WorldNotReady              UMETA(DisplayName = "World Not Ready"),
	NoGroundHit                UMETA(DisplayName = "No Ground Hit"),
	SlopeTooSteep              UMETA(DisplayName = "Slope Too Steep"),
	FootprintInvalid           UMETA(DisplayName = "Footprint Invalid"),
	HeightVariance             UMETA(DisplayName = "Height Variance"),
	NavigationUnavailable      UMETA(DisplayName = "Navigation Unavailable"),
	NavigationProjectionFailed UMETA(DisplayName = "Navigation Projection Failed"),
	HelicopterClearanceBlocked UMETA(DisplayName = "Helicopter Clearance Blocked"),
	RappelPathBlocked          UMETA(DisplayName = "Rappel Path Blocked"),
	ExclusionVolume            UMETA(DisplayName = "Exclusion Volume"),
	MissingInsertionAreaVolume UMETA(DisplayName = "Missing Insertion Area Volume"),
	OutsideInsertionArea       UMETA(DisplayName = "Outside Insertion Area")
};

inline const TCHAR* LexToString(EOBLandingZoneFailure Failure)
{
	switch (Failure)
	{
	case EOBLandingZoneFailure::None: return TEXT("None");
	case EOBLandingZoneFailure::WorldNotReady: return TEXT("WorldNotReady");
	case EOBLandingZoneFailure::NoGroundHit: return TEXT("NoGroundHit");
	case EOBLandingZoneFailure::SlopeTooSteep: return TEXT("SlopeTooSteep");
	case EOBLandingZoneFailure::FootprintInvalid: return TEXT("FootprintInvalid");
	case EOBLandingZoneFailure::HeightVariance: return TEXT("HeightVariance");
	case EOBLandingZoneFailure::NavigationUnavailable: return TEXT("NavigationUnavailable");
	case EOBLandingZoneFailure::NavigationProjectionFailed: return TEXT("NavigationProjectionFailed");
	case EOBLandingZoneFailure::HelicopterClearanceBlocked: return TEXT("HelicopterClearanceBlocked");
	case EOBLandingZoneFailure::RappelPathBlocked: return TEXT("RappelPathBlocked");
	case EOBLandingZoneFailure::ExclusionVolume: return TEXT("ExclusionVolume");
	case EOBLandingZoneFailure::MissingInsertionAreaVolume: return TEXT("MissingInsertionAreaVolume");
	case EOBLandingZoneFailure::OutsideInsertionArea: return TEXT("OutsideInsertionArea");
	default: return TEXT("Unknown");
	}
}

UENUM(BlueprintType)
enum class EOBExtractionCallPhase : uint8
{
	Ready         UMETA(DisplayName = "Ready"),
	FlareLaunched UMETA(DisplayName = "Flare Launched"),
	Waiting       UMETA(DisplayName = "Waiting"),
	Inbound       UMETA(DisplayName = "Inbound"),
	Landing       UMETA(DisplayName = "Landing"),
	Boarding      UMETA(DisplayName = "Boarding"),
	Departing     UMETA(DisplayName = "Departing"),
	Completed     UMETA(DisplayName = "Completed"),
	Cooldown      UMETA(DisplayName = "Cooldown"),
	Aborted       UMETA(DisplayName = "Aborted"),
	Expired       UMETA(DisplayName = "Expired")
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBLandingZoneResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Helicopter")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "Helicopter")
	FVector_NetQuantize GroundLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Helicopter")
	FTransform HoverTransform = FTransform::Identity;

	UPROPERTY(BlueprintReadOnly, Category = "Helicopter")
	float Score = TNumericLimits<float>::Max();

	/** Most frequent rejection reason when no valid candidate was found. */
	UPROPERTY(BlueprintReadOnly, Category = "Helicopter")
	EOBLandingZoneFailure Failure = EOBLandingZoneFailure::None;

	UPROPERTY(BlueprintReadOnly, Category = "Helicopter")
	int32 CandidatesEvaluated = 0;
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBTeamInsertionState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Insertion")
	uint8 TeamId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Insertion")
	EOBInsertionPhase Phase = EOBInsertionPhase::None;

	UPROPERTY(BlueprintReadOnly, Category = "Insertion")
	bool bHasRequestedLocation = false;

	UPROPERTY(BlueprintReadOnly, Category = "Insertion")
	FVector_NetQuantize RequestedLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Insertion")
	bool bHasResolvedLocation = false;

	UPROPERTY(BlueprintReadOnly, Category = "Insertion")
	FVector_NetQuantize ResolvedGroundLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Insertion")
	TObjectPtr<AOBInsertionHelicopter> Helicopter = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Insertion")
	float StateStartedServerTime = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Insertion")
	float SelectionDeadlineServerTime = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Insertion")
	int32 PassengerCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Insertion")
	int32 DeployedCount = 0;
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBExtractionCallState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Extraction")
	EOBExtractionCallPhase Phase = EOBExtractionCallPhase::Ready;

	UPROPERTY(BlueprintReadOnly, Category = "Extraction")
	uint8 CallingTeamId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Extraction")
	float CallStartedServerTime = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Extraction")
	float ArrivalServerTime = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Extraction")
	float BoardingEndsServerTime = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Extraction")
	int32 BoardedCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Extraction")
	TObjectPtr<AOBInsertionHelicopter> Helicopter = nullptr;
};
