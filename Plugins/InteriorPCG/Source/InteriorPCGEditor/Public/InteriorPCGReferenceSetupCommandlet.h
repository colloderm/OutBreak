// Copyright OutBreak. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "InteriorPCGReferenceSetupCommandlet.generated.h"

/** Audits the PostABundle LT1 reference and populates the five /Game/DevB/PCG example assets. */
UCLASS()
class UInteriorPCGReferenceSetupCommandlet final : public UCommandlet
{
	GENERATED_BODY()

public:
	UInteriorPCGReferenceSetupCommandlet();
	virtual int32 Main(const FString& Params) override;
};
