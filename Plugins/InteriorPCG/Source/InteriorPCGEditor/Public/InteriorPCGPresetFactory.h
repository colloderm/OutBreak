#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "InteriorPCGPresetFactory.generated.h"

UCLASS()
class INTERIORPCGEDITOR_API UInteriorPCGPresetFactory : public UFactory
{
	GENERATED_BODY()

public:
	UInteriorPCGPresetFactory();
	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual FString GetDefaultNewAssetName() const override;
};
