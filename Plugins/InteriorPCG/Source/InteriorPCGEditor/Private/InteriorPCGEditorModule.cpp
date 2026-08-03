#include "InteriorPCGVolumeDetails.h"
#include "InteriorPCGWallVolumeDetails.h"

#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"

class FInteriorPCGEditorModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
		PropertyEditor.RegisterCustomClassLayout(
			TEXT("InteriorPCGVolume"),
			FOnGetDetailCustomizationInstance::CreateStatic(&FInteriorPCGVolumeDetails::MakeInstance));
		PropertyEditor.RegisterCustomClassLayout(
			TEXT("InteriorPCGWallVolume"),
			FOnGetDetailCustomizationInstance::CreateStatic(&FInteriorPCGWallVolumeDetails::MakeInstance));
		PropertyEditor.NotifyCustomizationModuleChanged();
	}

	virtual void ShutdownModule() override
	{
		if (FPropertyEditorModule* PropertyEditor = FModuleManager::GetModulePtr<FPropertyEditorModule>(TEXT("PropertyEditor")))
		{
			PropertyEditor->UnregisterCustomClassLayout(TEXT("InteriorPCGVolume"));
			PropertyEditor->UnregisterCustomClassLayout(TEXT("InteriorPCGWallVolume"));
			PropertyEditor->NotifyCustomizationModuleChanged();
		}
	}
};

IMPLEMENT_MODULE(FInteriorPCGEditorModule, InteriorPCGEditor)
