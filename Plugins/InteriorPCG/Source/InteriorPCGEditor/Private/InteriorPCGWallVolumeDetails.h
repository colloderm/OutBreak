#pragma once

#include "InteriorPCGVolumeDetails.h"

class AInteriorPCGWallVolume;

class FInteriorPCGWallVolumeDetails final : public FInteriorPCGVolumeDetails
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	FReply OnGenerateWalls();
	FReply OnGenerateWallsAndInterior();
	FReply OnClearWalls();

	TWeakObjectPtr<AInteriorPCGWallVolume> WallVolume;
};
