

#pragma once

#include "CoreMinimal.h"
#include "OBGameModeBase.h"
#include "OBHomeGameMode.generated.h"

UCLASS()
class OUTBREAK_API AOBHomeGameMode : public AOBGameModeBase
{
	GENERATED_BODY()
	
public:
	AOBHomeGameMode();
	
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
};
