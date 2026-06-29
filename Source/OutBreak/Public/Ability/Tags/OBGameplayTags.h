// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "NativeGameplayTags.h"

/**
왜 존재하는가?
 - C++와 에디터 에셋이 공유하는 GameplayTag를 네이티브로 선언한다(문자열 오타 방지).
무엇을 저장하는가?
 - 프로젝트 전역 태그 심볼. 현재는 데미지 SetByCaller 태그.
멀티플레이 역할?
 - 모든 머신에 동일 등록. 복제 불필요.
 */
namespace OBGameplayTags
{
	// 데미지 GE에 수치를 실어 보내기 위한 SetByCaller 키.
	// GE_Damage의 Modifier Magnitude(Set By Caller)와 동일한 태그여야 한다.
	OUTBREAK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Damage);
	
	// 입력
	OUTBREAK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Weapon_Fire);
	OUTBREAK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Weapon_Reload);
	OUTBREAK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Weapon_Aim);
	
	// 상태
	OUTBREAK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dead);
	OUTBREAK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Reloading);
	OUTBREAK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Aiming);
	OUTBREAK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Weapon_Switching);
	
	// 연출(발사 - 총구 화염/사격음, 피격 - 탄착 이펙트/사운드 등).
	OUTBREAK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Weapon_Fire);
	OUTBREAK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Weapon_Impact);
	
	// 탄약 타입(인벤토리 탄약 풀의 키). 무기마다 자기 타입을 지정.
	OUTBREAK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ammo_AssaultRifle);
	OUTBREAK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ammo_SniperRifle);
	OUTBREAK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ammo_SMG);
	OUTBREAK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ammo_Shotgun);
	OUTBREAK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ammo_Pistol);
	
	OUTBREAK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Melee);
	
}
