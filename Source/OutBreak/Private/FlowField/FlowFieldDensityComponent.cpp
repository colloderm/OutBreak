// Copyright Epic Games, Inc. All Rights Reserved.

#include "FlowField/FlowFieldDensityComponent.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "FlowField/FlowFieldSubsystem.h"
#include "GameFramework/Actor.h"

UFlowFieldDensityComponent::UFlowFieldDensityComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UFlowFieldDensityComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		if (UFlowFieldSubsystem* FlowField = World->GetSubsystem<UFlowFieldSubsystem>())
		{
			FlowField->RegisterDensityComponent(this);
		}
	}
}

void UFlowFieldDensityComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UFlowFieldSubsystem* FlowField = World->GetSubsystem<UFlowFieldSubsystem>())
		{
			FlowField->UnregisterDensityComponent(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void UFlowFieldDensityComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	DecaySeparationVelocity(DeltaTime);
	DrawDensitySeparationDebug();
}

bool UFlowFieldDensityComponent::IsDensitySeparationEnabled() const
{
	return bEnableDensitySeparation && SeparationRadius > KINDA_SMALL_NUMBER;
}

FVector UFlowFieldDensityComponent::GetSeparationVelocity() const
{
	return bEnableDensitySeparation ? SeparationVelocity : FVector::ZeroVector;
}

void UFlowFieldDensityComponent::ApplySeparationVelocityDelta(const FVector& VelocityDelta)
{
	if (!bEnableDensitySeparation)
	{
		return;
	}

	SeparationVelocity += VelocityDelta;
	SeparationVelocity.Z = 0.0f;
	const float SeparationSpeed = SeparationVelocity.Size2D();
	if (MaxSeparationVelocity > 0.0f && SeparationSpeed > MaxSeparationVelocity)
	{
		SeparationVelocity *= MaxSeparationVelocity / SeparationSpeed;
	}
}

void UFlowFieldDensityComponent::ResetSeparationVelocity()
{
	SeparationVelocity = FVector::ZeroVector;
}

void UFlowFieldDensityComponent::DecaySeparationVelocity(const float DeltaTime)
{
	if (!bEnableDensitySeparation)
	{
		SeparationVelocity = FVector::ZeroVector;
		return;
	}

	if (SeparationVelocityDamping > 0.0f)
	{
		SeparationVelocity = FMath::VInterpTo(SeparationVelocity, FVector::ZeroVector, DeltaTime, SeparationVelocityDamping);
	}
}

void UFlowFieldDensityComponent::DrawDensitySeparationDebug() const
{
	const AActor* Owner = GetOwner();
	if (!bDrawDensitySeparationDebug || !bEnableDensitySeparation || Owner == nullptr || GetWorld() == nullptr)
	{
		return;
	}

	const FVector Location = Owner->GetActorLocation();
	DrawDebugCircle(GetWorld(), Location, SeparationRadius, 20, FColor::Cyan, false, 0.0f, 0, 1.0f, FVector::ForwardVector, FVector::RightVector, false);
	if (!SeparationVelocity.IsNearlyZero())
	{
		DrawDebugDirectionalArrow(GetWorld(), Location, Location + SeparationVelocity * 0.15f, 12.0f, FColor::Yellow, false, 0.0f, 0, 1.5f);
	}
}
