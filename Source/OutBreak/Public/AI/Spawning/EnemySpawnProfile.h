#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "EnemySpawnProfile.generated.h"

class AEnemyCharacter;
class UAnimMontage;

UCLASS(BlueprintType, Const)
class OUTBREAK_API UEnemySpawnProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy")
	TSubclassOf<AEnemyCharacter> EnemyClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemy")
	FName PoolKey = TEXT("DefaultZombie");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation")
	TObjectPtr<UAnimMontage> SpawnMontage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation", meta=(ClampMin="0.0", Units="s"))
	float PresentationDuration = 1.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation", meta=(Units="cm"))
	float GroundOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pool", meta=(ClampMin="0"))
	int32 WarmPoolCount = 8;
};
