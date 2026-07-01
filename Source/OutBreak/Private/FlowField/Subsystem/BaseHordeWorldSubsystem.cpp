// Fill out your copyright notice in the Description page of Project Settings.


#include "FlowField/Subsystem/BaseHordeWorldSubsystem.h"

#include "FlowField/Settings/FlowFieldSettings.h"

void UBaseHordeWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	Settings = const_cast<UFlowFieldSettings*>(GetDefault<UFlowFieldSettings>());
	
	
}
