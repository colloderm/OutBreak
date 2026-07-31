// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "EnemyAsset.generated.h"

class USoundCue;
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

USTRUCT(BlueprintType)
struct FTraversalSetting
{
	GENERATED_BODY()
	
	/* Traversal Values */
	
	/* Vault X:75, Z:110~112 (cm.) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Traversal|Vault")
	float VaultSpan = 75.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Traversal|Vault")
	float VaultMinHeight = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Traversal|Vault")
	float VaultMaxHeight = 112.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Traversal|Vault|Animation")
	TObjectPtr<UAnimMontage> VaultMontage;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Traversal|Vault|Animation")
	float VaultPlayRate = 0.7f;

	/* Mantle */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Traversal")
	float MantleMinHeight = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Traversal")
	float MantleMaxHeight = 240.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Traversal|Mantle|Animation")
	TObjectPtr<UAnimMontage> MantleMontage;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Traversal|Mantle|Animation")
	float MantlePlayRate = 0.7f;
	
	/* Climb Up */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Traversal|ClimbUp")
	float ClimbUpMinHeight = 360.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Traversal|ClimbUp")
	float ClimbUpMaxHeight = 370.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Traversal|ClimbUp|Animation")
	TObjectPtr<UAnimMontage> ClimbUpMontage;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Traversal|ClimbUp|Animation")
	float ClimbUpPlayRate = 0.7f;
};


USTRUCT(BlueprintType)
struct FEnemyPerception
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sight|Config")
	float SightDegree = 70.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sight|Config")
	float SightRadius = 2500.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sight|Config")
	float LoseSightRadius = 3500.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sight|Config")
	float MaxAge = 3.f;
	
};

USTRUCT(BlueprintType)
struct FEnemySoundAsset
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	TObjectPtr<USoundCue> ZombieCryingSound;
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
	const FTraversalSetting* GetTraversalSetting() const { return &TraversalSetting; } 
	const FEnemySoundAsset* GetSoundAssets() const { return &SoundAssets; }
	
private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess="true"));
	FEnemyLimbMesh LimbMeshes;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess="true"));
	FEnemyPhysicalReact PhysicalReact;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess="true"));
	FTraversalSetting TraversalSetting;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess="true"));
	FEnemySoundAsset SoundAssets;
	
};
