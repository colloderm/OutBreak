// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "OBTraitTreeLayoutData.generated.h"

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBTraitNodeLayout
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait|Layout")
	FGameplayTag NodeId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait|Layout")
	FVector2D Position = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait|Layout")
	int32 Layer = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait|Layout")
	int32 SortOrder = 0;
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FOBTraitConnectionLayout
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait|Layout")
	FGameplayTag FromNodeId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait|Layout")
	FGameplayTag ToNodeId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait|Layout")
	TArray<FVector2D> ControlPoints;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait|Layout")
	bool bVisible = true;
};

UCLASS(BlueprintType, Const)
class OUTBREAK_API UOBTraitTreeLayoutData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	const FOBTraitNodeLayout* FindNodeLayout(FGameplayTag NodeId) const;
	const FOBTraitConnectionLayout* FindConnectionLayout(FGameplayTag FromNodeId, FGameplayTag ToNodeId) const;

	int32 GetLayoutVersion() const { return LayoutVersion; }
	FPrimaryAssetId GetTargetTreeId() const { return TargetTreeId; }
	const TArray<FOBTraitConnectionLayout>& GetConnectionLayouts() const { return ConnectionLayouts; }

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait|Layout", Meta = (ClampMin = "1", AllowPrivateAccess = "true"))
	int32 LayoutVersion = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait|Layout", Meta = (AllowPrivateAccess = "true"))
	FPrimaryAssetId TargetTreeId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait|Layout", Meta = (AllowPrivateAccess = "true"))
	TArray<FOBTraitNodeLayout> NodeLayouts;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait|Layout", Meta = (AllowPrivateAccess = "true"))
	TArray<FOBTraitConnectionLayout> ConnectionLayouts;
};
