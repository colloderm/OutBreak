#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "OBGenerateDataTablesCommandlet.generated.h"

UCLASS()
class OUTBREAKEDITOR_API UOBGenerateDataTablesCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UOBGenerateDataTablesCommandlet();
	virtual int32 Main(const FString& Params) override;
};
