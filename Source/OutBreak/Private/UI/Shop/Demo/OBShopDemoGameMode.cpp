// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Shop/Demo/OBShopDemoGameMode.h"

#include "UI/Shop/Demo/OBShopDemoPawn.h"
#include "UI/Shop/Demo/OBShopDemoPlayerController.h"

AOBShopDemoGameMode::AOBShopDemoGameMode()
{
	DefaultPawnClass = AOBShopDemoPawn::StaticClass();
	PlayerControllerClass = AOBShopDemoPlayerController::StaticClass();
	bUseSeamlessTravel = false;
}
