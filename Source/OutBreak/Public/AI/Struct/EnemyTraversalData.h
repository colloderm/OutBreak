#pragma once

#include "CoreMinimal.h"

UENUM(Blueprintable)
enum class ETraversalType : uint8
{
	Walk UMETA(DisplayName="Walk"),
	Drop UMETA(DisplayName="Drop"),
	Vault UMETA(DisplayName="Vault"),
	Mantle UMETA(DisplayName="Mantle"),
	ClimbUp UMETA(DisplayName="ClimbUp"),
};

UENUM(Blueprintable)
enum class ETraversalLinkType : uint8
{
	None UMETA(DisplayName="None"),
	Vault UMETA(DisplayName="Vault"),
	Mantle UMETA(DisplayName="Mantle"),
	ClimbUp UMETA(DisplayName="ClimbUp"),
};