// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Struct/FlowFieldNavTypes.h"
#include "Navigation/NavLinkProxy.h"
#include "FlowFieldNavLinkProxy.generated.h"




/**
 * 
 */
UCLASS(BlueprintType)
class OUTBREAK_API AFlowFieldNavLinkProxy : public ANavLinkProxy
{
	GENERATED_BODY()
	
	
	
	
public:
	/** Optional designer override used when automatic height/link classification is not enough. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|Traversal Bake")
	EFlowFieldTraversalBakeType TraversalBakeOverride = EFlowFieldTraversalBakeType::None;

	/** Forces the baked wall-rush direction. Zero means the Flow Field derives it from the link endpoints. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|Traversal Bake")
	FVector WallForwardOverride = FVector::ZeroVector;

	/** Marks this link as requiring a Horde Tower even if its height also fits a simple climb range. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|Traversal Bake")
	bool bRequiresHordeTower = false;

	/*네비게이션 Dirty 발생 후 재빌드 시 NavLink도 호출하여서 참조 레페런스 노드를 재할당*/
	void DirtyRebuild(NavNodeRef A, NavNodeRef B);
	
	NavNodeRef GetAntipodeNode(const NavNodeRef A) const
	{
		if (EndPoint[0] == A)
		{
			return EndPoint[1];
		}
		else if (EndPoint[1] == A)
		{
			return EndPoint[0];
		}
		return 777888999;
	}
	
	virtual void EntryNavLink(const NavNodeRef Entry);
	
	
	
protected:
	/* [0] = A, [1] = B*/
	NavNodeRef EndPoint[2];
};
