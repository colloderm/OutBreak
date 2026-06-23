// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Tags/OBGameplayTags.h"

namespace OBGameplayTags
{
	// 실제 태그 문자열을 정의·등록한다. 엔진 시작 시 자동 등록되어
	// 에디터의 GameplayTag 목록에도 "SetByCaller.Damage"로 나타난다.
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_Damage, "SetByCaller.Damage");
	
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Weapon_Fire, "InputTag.Weapon.Fire");
	
	UE_DEFINE_GAMEPLAY_TAG(State_Dead, "State.Dead");
	
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Weapon_Fire,   "GameplayCue.Weapon.Fire");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Weapon_Impact, "GameplayCue.Weapon.Impact");
}
