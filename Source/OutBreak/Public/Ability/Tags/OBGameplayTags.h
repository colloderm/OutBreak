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
	
	OUTBREAK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Weapon_Fire);
	OUTBREAK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Weapon_Reload);
	OUTBREAK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Weapon_Aim);
	
	OUTBREAK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dead);
	OUTBREAK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Reloading);
	OUTBREAK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Aiming);
	
	// 발사 연출(총구 화염/사격음).
	OUTBREAK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Weapon_Fire);
	// 피격 연출(탄착 이펙트/사운드).
	OUTBREAK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Weapon_Impact);
	
}
