// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Shop/Demo/OBShopDemoPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"

AOBShopDemoPawn::AOBShopDemoPawn()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
	CameraComponent->SetupAttachment(SceneRoot);
	CameraComponent->SetRelativeLocation(FVector(-260.0, 0.0, 120.0));
	CameraComponent->SetRelativeRotation(FRotator(-12.0, 0.0, 0.0));
}
