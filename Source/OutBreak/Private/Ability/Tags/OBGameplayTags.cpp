// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Tags/OBGameplayTags.h"

namespace OBGameplayTags
{
	// 실제 태그 문자열을 정의·등록한다. 엔진 시작 시 자동 등록되어
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_Damage,			"SetByCaller.Damage");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_Heal,			"SetByCaller.Heal");
	
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Weapon_Fire,		"InputTag.Weapon.Fire");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Weapon_Reload,		"InputTag.Weapon.Reload");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Weapon_Aim,			"InputTag.Weapon.Aim");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Consumable_Heal,    "InputTag.Consumable.Heal");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Consumable_Grenade, "InputTag.Consumable.Grenade");
	
	UE_DEFINE_GAMEPLAY_TAG(State_Dead,					"State.Dead");
	UE_DEFINE_GAMEPLAY_TAG(State_Downed,				"State.Downed");
	UE_DEFINE_GAMEPLAY_TAG(State_Reloading,				"State.Reloading");
	UE_DEFINE_GAMEPLAY_TAG(State_Aiming,				"State.Aiming");
	UE_DEFINE_GAMEPLAY_TAG(State_Weapon_Switching,		"State.Weapon.Switching");
	UE_DEFINE_GAMEPLAY_TAG(State_UsingConsumable,		"State.UsingConsumable");
	UE_DEFINE_GAMEPLAY_TAG(State_Melee_Attacking,		"State.Melee.Attacking");
	
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Weapon_Fire,		"GameplayCue.Weapon.Fire");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Weapon_Impact,	"GameplayCue.Weapon.Impact");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Melee_Swing,		"GameplayCue.Melee.Swing");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Melee_Impact,	"GameplayCue.Melee.Impact");
	
	UE_DEFINE_GAMEPLAY_TAG(Ammo_AssaultRifle,			"Ammo.AssaultRifle");
	UE_DEFINE_GAMEPLAY_TAG(Ammo_SniperRifle,			"Ammo.SniperRifle");
	UE_DEFINE_GAMEPLAY_TAG(Ammo_SMG,					"Ammo.SMG");
	UE_DEFINE_GAMEPLAY_TAG(Ammo_Shotgun,				"Ammo.Shotgun");
	UE_DEFINE_GAMEPLAY_TAG(Ammo_Pistol,					"Ammo.Pistol");
	
	UE_DEFINE_GAMEPLAY_TAG(Melee,						"Melee");

	UE_DEFINE_GAMEPLAY_TAG(Stat_Weapon_Damage,             "Stat.Weapon.Damage");
	UE_DEFINE_GAMEPLAY_TAG(Stat_Weapon_Range,              "Stat.Weapon.Range");
	UE_DEFINE_GAMEPLAY_TAG(Stat_Weapon_Mobility,           "Stat.Weapon.Mobility");
	UE_DEFINE_GAMEPLAY_TAG(Stat_Weapon_HeadshotMultiplier, "Stat.Weapon.HeadshotMultiplier");
	UE_DEFINE_GAMEPLAY_TAG(Stat_Weapon_MagazineSize,       "Stat.Weapon.MagazineSize");
	UE_DEFINE_GAMEPLAY_TAG(Stat_Weapon_RoundsPerMinute,    "Stat.Weapon.RoundsPerMinute");
	UE_DEFINE_GAMEPLAY_TAG(Stat_Weapon_RecoilVertical,     "Stat.Weapon.Recoil.Vertical");
	UE_DEFINE_GAMEPLAY_TAG(Stat_Weapon_RecoilHorizontal,   "Stat.Weapon.Recoil.Horizontal");
	UE_DEFINE_GAMEPLAY_TAG(Stat_Weapon_RecoilRecovery,     "Stat.Weapon.Recoil.Recovery");
	UE_DEFINE_GAMEPLAY_TAG(Stat_Weapon_ADSFOV,             "Stat.Weapon.ADS.FOV");
	UE_DEFINE_GAMEPLAY_TAG(Stat_Weapon_ADSSpeed,           "Stat.Weapon.ADS.Speed");
	UE_DEFINE_GAMEPLAY_TAG(Stat_Weapon_ADSRecoil,          "Stat.Weapon.ADS.Recoil");
	UE_DEFINE_GAMEPLAY_TAG(Stat_Weapon_SpreadBase,         "Stat.Weapon.Spread.Base");
	UE_DEFINE_GAMEPLAY_TAG(Stat_Weapon_SpreadADS,          "Stat.Weapon.Spread.ADS");
	UE_DEFINE_GAMEPLAY_TAG(Stat_Weapon_SpreadMoving,       "Stat.Weapon.Spread.Moving");
	UE_DEFINE_GAMEPLAY_TAG(Stat_Melee_Reach,               "Stat.Melee.Reach");
	UE_DEFINE_GAMEPLAY_TAG(Stat_Melee_SweepRadius,         "Stat.Melee.SweepRadius");
	UE_DEFINE_GAMEPLAY_TAG(Stat_Melee_Arc,                 "Stat.Melee.Arc");
	UE_DEFINE_GAMEPLAY_TAG(Stat_Melee_HitTime,             "Stat.Melee.HitTime");
	UE_DEFINE_GAMEPLAY_TAG(Stat_Melee_AttackDuration,      "Stat.Melee.AttackDuration");
	UE_DEFINE_GAMEPLAY_TAG(Stat_Melee_StaminaCost,         "Stat.Melee.StaminaCost");
	UE_DEFINE_GAMEPLAY_TAG(Stat_Player_MaxHealth,          "Stat.Player.MaxHealth");
	UE_DEFINE_GAMEPLAY_TAG(Stat_Player_MaxStamina,         "Stat.Player.MaxStamina");
	UE_DEFINE_GAMEPLAY_TAG(Stat_Player_MoveSpeed,          "Stat.Player.MoveSpeed");
	UE_DEFINE_GAMEPLAY_TAG(Stat_Player_CarryCapacity,      "Stat.Player.CarryCapacity");
	UE_DEFINE_GAMEPLAY_TAG(Stat_Player_RecoilControl,      "Stat.Player.RecoilControl");
	UE_DEFINE_GAMEPLAY_TAG(Stat_Player_AimStability,       "Stat.Player.AimStability");
	UE_DEFINE_GAMEPLAY_TAG(Stat_Player_MeleePower,         "Stat.Player.MeleePower");
	UE_DEFINE_GAMEPLAY_TAG(Stat_Player_Armor,              "Stat.Player.Armor");
	
	UE_DEFINE_GAMEPLAY_TAG(Item_Bandage,				"Item.Bandage");
	UE_DEFINE_GAMEPLAY_TAG(Item_Grenade,				"Item.Grenade");
	
	UE_DEFINE_GAMEPLAY_TAG(Item_Weapon_AssaultRifle,	"Item.Weapon.AssaultRifle");
	UE_DEFINE_GAMEPLAY_TAG(Item_Weapon_SniperRifle,		"Item.Weapon.SniperRifle");
	UE_DEFINE_GAMEPLAY_TAG(Item_Weapon_Shotgun,			"Item.Weapon.Shotgun");
	UE_DEFINE_GAMEPLAY_TAG(Item_Weapon_Pistol,			"Item.Weapon.Pistol");
	UE_DEFINE_GAMEPLAY_TAG(Item_Weapon_FireAxe,			"Item.Weapon.FireAxe");
	
	UE_DEFINE_GAMEPLAY_TAG(Item_Valuable_Watch,			"Item.Valuable.Watch");
	UE_DEFINE_GAMEPLAY_TAG(Item_Valuable_Gold,			"Item.Valuable.Gold");
	
	UE_DEFINE_GAMEPLAY_TAG(Item_Material_Scrap,			"Item.Material.Scrap");
	UE_DEFINE_GAMEPLAY_TAG(Item_Material_Cloth,			"Item.Material.Cloth");

	UE_DEFINE_GAMEPLAY_TAG(AttachmentSlot_Optic,           "AttachmentSlot.Optic");
	UE_DEFINE_GAMEPLAY_TAG(AttachmentSlot_Muzzle,          "AttachmentSlot.Muzzle");
	UE_DEFINE_GAMEPLAY_TAG(AttachmentSlot_Magazine,        "AttachmentSlot.Magazine");
	UE_DEFINE_GAMEPLAY_TAG(AttachmentSlot_Stock,           "AttachmentSlot.Stock");
	UE_DEFINE_GAMEPLAY_TAG(AttachmentSlot_Grip,            "AttachmentSlot.Grip");
	UE_DEFINE_GAMEPLAY_TAG(AttachmentSlot_Melee_Mod,       "AttachmentSlot.Melee.Mod");
	
	
	UE_DEFINE_GAMEPLAY_TAG(TAG_StateTree_Event_TargetSighted, "StateTree.Vent.TargetSighted");
	UE_DEFINE_GAMEPLAY_TAG(TAG_StateTree_Event_MemoryUpdated, "StateTree.Event.MemoryUpdated");
	
}
