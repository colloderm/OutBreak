// Fill out your copyright notice in the Description page of Project Settings.

#include "Item/Data/OBItemTypes.h"

namespace OBItemStacks
{
	void Add(TArray<FOBItemStack>& Stacks, const FGameplayTag& ItemTag, int32 Count)
	{
		if (!ItemTag.IsValid() || Count <= 0) return;

		// 컨테이너는 칸 개념이 없어서 MaxStack을 적용하지 않고 태그당 한 항목으로 합친다.
		for (FOBItemStack& S : Stacks)
		{
			if (S.ItemTag == ItemTag)
			{
				S.Count += Count;
				return;
			}
		}
		Stacks.Emplace(ItemTag, Count);
	}
}