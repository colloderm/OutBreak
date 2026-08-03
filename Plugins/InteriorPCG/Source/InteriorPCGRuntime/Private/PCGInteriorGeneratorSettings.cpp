#include "PCGInteriorGeneratorSettings.h"

#include "InteriorPCGVolume.h"
#include "PCGCommon.h"
#include "PCGComponent.h"
#include "PCGContext.h"

TArray<FPCGPinProperties> UPCGInteriorGeneratorSettings::InputPinProperties() const
{
	return { FPCGPinProperties(PCGPinConstants::DefaultInputLabel, EPCGDataType::Any, true, true) };
}

TArray<FPCGPinProperties> UPCGInteriorGeneratorSettings::OutputPinProperties() const
{
	return { FPCGPinProperties(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Any) };
}

FPCGElementPtr UPCGInteriorGeneratorSettings::CreateElement() const
{
	return MakeShared<FPCGInteriorGeneratorElement>();
}

bool FPCGInteriorGeneratorElement::ExecuteInternal(FPCGContext* Context) const
{
	check(Context);
	Context->OutputData = Context->InputData;

	UPCGComponent* SourceComponent = Cast<UPCGComponent>(Context->ExecutionSource.Get());
	if (SourceComponent)
	{
		SourceComponent = SourceComponent->GetOriginalComponent();
	}

	if (AInteriorPCGVolume* Volume = SourceComponent ? Cast<AInteriorPCGVolume>(SourceComponent->GetOwner()) : nullptr)
	{
		Volume->ExecuteGraphGeneration();
	}

	return true;
}
