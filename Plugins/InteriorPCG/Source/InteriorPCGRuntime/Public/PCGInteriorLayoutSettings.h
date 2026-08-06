// Copyright OutBreak. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "InteriorPCGCoreTypes.h"
#include "PCGInteriorLayoutSettings.generated.h"

class UInteriorPCGGenerationProfile;

namespace InteriorPCGPinConstants
{
	inline const FName Structure = TEXT("Structure");
	inline const FName Interior = TEXT("Interior");
	inline const FName Rooms = TEXT("Rooms");
}

/** PCG graph source node that emits semantic placement points instead of hard-coding mesh spawners. */
UCLASS(BlueprintType, ClassGroup = "Procedural", meta = (DisplayName = "Generate Building + Interior Layout"))
class INTERIORPCGRUNTIME_API UPCGInteriorLayoutSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (PCG_Overridable))
	TObjectPtr<UInteriorPCGGenerationProfile> Profile;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (PCG_Overridable))
	FInteriorPCGGenerationOptions GenerationOptions;

#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return TEXT("InteriorPCGLayout"); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("InteriorPCG", "LayoutNodeTitle", "Generate Building + Interior Layout"); }
	virtual FText GetNodeTooltipText() const override
	{
		return NSLOCTEXT("InteriorPCG", "LayoutNodeTooltip", "Generates deterministic structure, room, and interior semantic points from an Interior PCG profile.");
	}
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
};

class FPCGInteriorLayoutElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
};
