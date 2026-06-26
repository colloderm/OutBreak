// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Tags/OBGameplayTags.h"

namespace OBGameplayTags
{
	// 실제 태그 문자열을 정의·등록한다. 엔진 시작 시 자동 등록되어
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_Damage, "SetByCaller.Damage");
	
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Weapon_Fire,	"InputTag.Weapon.Fire");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Weapon_Reload,	"InputTag.Weapon.Reload");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Weapon_Aim,		"InputTag.Weapon.Aim");
	
	UE_DEFINE_GAMEPLAY_TAG(State_Dead,		"State.Dead");
	UE_DEFINE_GAMEPLAY_TAG(State_Reloading, "State.Reloading");
	UE_DEFINE_GAMEPLAY_TAG(State_Aiming,	"State.Aiming");
	
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Weapon_Fire,		"GameplayCue.Weapon.Fire");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Weapon_Impact,	"GameplayCue.Weapon.Impact");
	
	UE_DEFINE_GAMEPLAY_TAG(Ammo_AssaultRifle, "Ammo.AssaultRifle");
	UE_DEFINE_GAMEPLAY_TAG(Ammo_SniperRifle,  "Ammo.SniperRifle");
	UE_DEFINE_GAMEPLAY_TAG(Ammo_SMG,          "Ammo.SMG");
	UE_DEFINE_GAMEPLAY_TAG(Ammo_Shotgun,      "Ammo.Shotgun");
	UE_DEFINE_GAMEPLAY_TAG(Ammo_Pistol,       "Ammo.Pistol");
	
	UE_DEFINE_GAMEPLAY_TAG(Melee, "Melee");
}
