#include "InteriorPCGPresetFactory.h"

#include "InteriorPCGPreset.h"

UInteriorPCGPresetFactory::UInteriorPCGPresetFactory()
{
	SupportedClass = UInteriorPCGPreset::StaticClass();
	bCreateNew = true;
	bEditAfterNew = false;
}

UObject* UInteriorPCGPresetFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UInteriorPCGPreset>(InParent, InClass, InName, Flags | RF_Transactional);
}

FString UInteriorPCGPresetFactory::GetDefaultNewAssetName() const
{
	return TEXT("PCGPreset_Interior");
}
