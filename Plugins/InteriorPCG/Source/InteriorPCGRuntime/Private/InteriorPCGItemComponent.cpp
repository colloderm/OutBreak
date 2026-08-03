#include "InteriorPCGItemComponent.h"

UInteriorPCGItemComponent::UInteriorPCGItemComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bAutoActivate = true;
}
