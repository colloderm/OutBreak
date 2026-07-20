// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "EnemyAsset.generated.h"


class UCurveFloat;
class UPhysicalMaterial;
class UStaticMesh;


USTRUCT(BlueprintType)
struct FEnemyLimbMesh
{
	GENERATED_BODY()
	
	/* Physical Mesh */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UStaticMesh> SM_Arm_R;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UStaticMesh> SM_Arm_L;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UStaticMesh> SM_Leg_R;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UStaticMesh> SM_Leg_L;
};

USTRUCT(BlueprintType)
struct FEnemyPhysicalReact
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UCurveFloat> ReactCurveFloat;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float ReactScale;
	
	/* Physical Material */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UPhysicalMaterial> PM_Head;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UPhysicalMaterial> PM_Torso;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UPhysicalMaterial> PM_Arm_R;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UPhysicalMaterial> PM_Arm_L;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UPhysicalMaterial> PM_Leg_R;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UPhysicalMaterial> PM_Leg_L;
	
};

/**
 * 
 */
UCLASS(BlueprintType, Const)
class OUTBREAK_API UEnemyAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:
	const FEnemyPhysicalReact* GetPhysicalReact() const { return &PhysicalReact; } 
	const FEnemyLimbMesh* GetLimbMeshes() const { return &LimbMeshes; } 
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly);
	FEnemyLimbMesh LimbMeshes;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FEnemyPhysicalReact PhysicalReact;
	
};
