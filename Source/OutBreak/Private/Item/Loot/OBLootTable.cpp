// Fill out your copyright notice in the Description page of Project Settings.


#include "Item/Loot/OBLootTable.h"

void UOBLootTable::Roll(FRandomStream& Stream, TArray<FOBItemStack>& OutItems) const
{
	OutItems.Reset();
	if (Entries.IsEmpty()) return;
	
	float TotalWeight = 0.f;
	for (const FOBLootEntry& Entry : Entries)
	{
		if (Entry.ItemTag.IsValid())
		{
			TotalWeight += FMath::Max(0.f, Entry.Weight);
		}
	}
	if (TotalWeight <= 0.f) return; // 전부 가중치 0 -> 나올게 없다
	
	const int32 Lo = FMath::Min(MinRolls, MaxRolls);
	const int32 Hi = FMath::Max(MinRolls, MaxRolls);
	const int32 Rolls = Stream.RandRange(Lo, Hi);
	
	for (int32 i = 0; i < Rolls; ++i)
	{
		float Pick = Stream.FRandRange(0.f, TotalWeight);
		
		for (const FOBLootEntry& Entry : Entries)
		{
			const float W = Entry.ItemTag.IsValid() ? FMath::Max(0.f, Entry.Weight) : 0.f;
			if (W <= 0.f) continue;
			
			Pick -= W;
			if (Pick > 0.f) continue;
			
			const int32 CountLo = FMath::Min(Entry.MinCount, Entry.MaxCount);
			const int32 CountHi = FMath::Max(Entry.MinCount, Entry.MaxCount);
			OBItemStacks::Add(OutItems, Entry.ItemTag, Stream.RandRange(CountLo, CountHi));
			break;
		}
	}
}
