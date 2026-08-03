#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "PCGInteriorGeneratorSettings.generated.h"

UCLASS(BlueprintType, ClassGroup = (Procedural))
class INTERIORPCGRUNTIME_API UPCGInteriorGeneratorSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("InteriorGenerator")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("InteriorPCG", "GeneratorNodeTitle", "Generate Editable Interior Actors"); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spawner; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
};

class FPCGInteriorGeneratorElement : public IPCGElement
{
protected:
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
