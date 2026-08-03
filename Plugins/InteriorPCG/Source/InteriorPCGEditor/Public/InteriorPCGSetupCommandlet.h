#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "InteriorPCGSetupCommandlet.generated.h"

UCLASS()
class INTERIORPCGEDITOR_API UInteriorPCGSetupCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UInteriorPCGSetupCommandlet();
	virtual int32 Main(const FString& Params) override;
};
