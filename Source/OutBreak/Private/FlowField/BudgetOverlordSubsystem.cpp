// Fill out your copyright notice in the Description page of Project Settings.


#include "FlowField/BudgetOverlordSubsystem.h"

void UBudgetOverlordSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

TStatId UBudgetOverlordSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBudgetOverlordSubsystem, STATGROUP_Tickables);
}
