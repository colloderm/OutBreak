#pragma once

#include "IDetailCustomization.h"

class AInteriorPCGVolume;

class FInteriorPCGVolumeDetails : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	FReply OnGenerateRandom();
	FReply OnGeneratePreset();
	FReply OnSaveNewPreset();
	FReply OnUpdatePreset();
	FReply OnClearGenerated();
	FReply OnRegisterSelectedActors();

	TWeakObjectPtr<AInteriorPCGVolume> Volume;
};
