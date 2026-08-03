#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Equipment/Data/OBEquipmentTypes.h"
#include "OBEquipmentData.generated.h"

class AActor;
class UOBAbilitySet;

// Extension asset for non-weapon equipment such as helmets and armor.
// Display/carry/shop data remains in UOBItemDefinition.
UCLASS(BlueprintType, Const)
class OUTBREAK_API UOBEquipmentData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	EOBEquipmentSlot EquipmentSlot = EOBEquipmentSlot::None;

	// Optional runtime actor used when the equipment needs a replicated visual or behavior actor.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	TSoftClassPtr<AActor> EquipmentActorClass;

	// Storage capacity supplied by an equipped backpack.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment|Backpack",
		Meta = (ClampMin = "0", EditCondition = "EquipmentSlot == EOBEquipmentSlot::Backpack", EditConditionHides))
	int32 BackpackSlotCount = 20;

	// Optional GAS grants for armor/passive equipment.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment|GAS")
	TObjectPtr<UOBAbilitySet> AbilitySet;
};
