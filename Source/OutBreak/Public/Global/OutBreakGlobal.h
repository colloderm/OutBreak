#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OutBreakGlobal.generated.h"

class AActor;
class USoundCue;

/**
 * 프로젝트 전역에서 사용할 수 있는 블루프린트 함수 라이브러리.
 *
 * 객체를 생성하지 않고 블루프린트에서 직접 호출합니다.
 */
UCLASS()
class OUTBREAK_API UOutBreakGlobal
	: public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 지정한 위치에서 사운드를 재생하고 AI Hearing에 소음 이벤트를 보고합니다.
	 */
	UFUNCTION(
		BlueprintCallable,
		Category = "OutBreak|Audio",
		meta = (
			DisplayName = "Play Sound And Report Noise",
			WorldContext = "WorldContextObject",
			DefaultToSelf = "Instigator",
			AdvancedDisplay = "NoiseTag,NoiseRangeScale"
		))
	static void PlaySoundAndReportNoise(
		UObject* WorldContextObject,
		USoundCue* SoundCue,
		const FVector& Location,
		AActor* Instigator,
		FName NoiseTag = NAME_None,
		float NoiseRangeScale = 1.0f);
};