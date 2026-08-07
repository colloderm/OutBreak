// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "OBExpeditionMapData.generated.h"

class UTexture2D;

UCLASS(BlueprintType)
class OUTBREAK_API UOBExpeditionMapData : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:
	// 맵 선택 UI 표시명.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map")
	FText DisplayName;

	// 실제 로드할 레벨(ServerTravel 대상).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map")
	TSoftObjectPtr<UWorld> Level;
	
	// 지역 카드 썸네일.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map")
	TObjectPtr<UTexture2D> Thumbnail;

	// 난이도(★ 개수 표시용).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map", meta = (ClampMin = "0", ClampMax = "5"))
	int32 DifficultyStars = 1;

	// 카드 부제/설명(선택).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map")
	FText Description;

	// [핵심] 이 세션의 총 정원(PvPvE 전체). 기본 맵 12. 큰 맵은 이 값을 올림.
	// - GameSession->MaxPlayers로 들어가 PreLogin 단계에서 초과 접속을 거부.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Session", meta = (ClampMin = "1", ClampMax = "100"))
	int32 MaxSessionPlayers = 12;

	// 세션 길이(초). 15~30분 범위. 기본 1200(20분).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Session", meta = (ClampMin = "60"))
	int32 SessionLength = 1200;
	
	// [알파 IP-direct 테스트] Level 대신 테스트 데디 서버로 접속할 때만 켠다.
	// 기본값은 false이므로 에디터에서 Level을 지정하면 해당 맵을 로컬로 연다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Session|Test")
	bool bUseTestServerAddress = false;

	// bUseTestServerAddress가 true일 때 접속할 주소(예: "127.0.0.1:7777").
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Session|Test")
	FString TestServerAddress;

	// [개인 탈출] 맵별 개인 탈출구 활성 시작 경과초.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Session|Extraction", meta = (ClampMin = "0"))
	int32 PersonalActiveStartSec = 0;

	// [개인 탈출] 활성 종료 경과초(0=끝까지). 먼 거리 맵일수록 늘려 도달 여유 확보.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Session|Extraction", meta = (ClampMin = "0"))
	int32 PersonalActiveEndSec = 600;
	
	// ===== 전체 지도 =====

	// 에디터에서 미리 구운 탑다운 이미지.
	// World Partition이라 런타임 SceneCapture는 못 쓴다(언로드된 셀이 빈칸으로 찍힌다).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map|WorldMap")
	TObjectPtr<UTexture2D> WorldMapTexture;

	// 지도 이미지가 덮는 월드 영역의 중심(XY).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map|WorldMap")
	FVector2D WorldMapCenter = FVector2D::ZeroVector;

	// 지도 이미지가 덮는 월드 크기(cm). 캡처 액터의 Ortho Width와 같아야 한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map|WorldMap", meta = (ClampMin = "1.0"))
	FVector2D WorldMapSize = FVector2D(100000.0, 100000.0);

	// 월드 좌표 → 지도 UV(좌상단 0,0 / 우하단 1,1).
	// SceneCapture2D를 Pitch=-90 / Yaw=0 / Orthographic으로 찍은 이미지 기준이다:
	//   이미지 오른쪽 = 월드 +Y, 이미지 위쪽 = 월드 +X.
	FVector2D WorldToMapUV(const FVector& WorldLocation) const
	{
		const double SizeX = FMath::Max(1.0, WorldMapSize.X);
		const double SizeY = FMath::Max(1.0, WorldMapSize.Y);

		const double MinX = WorldMapCenter.X - SizeX * 0.5;
		const double MinY = WorldMapCenter.Y - SizeY * 0.5;

		return FVector2D(
			(WorldLocation.Y - MinY) / SizeY,
			1.0 - (WorldLocation.X - MinX) / SizeX);
	}

	/** Inverse of WorldToMapUV. Z is intentionally supplied by server-side ground scanning. */
	FVector2D MapUVToWorldXY(const FVector2D& UV) const
	{
		const double SizeX = FMath::Max(1.0, WorldMapSize.X);
		const double SizeY = FMath::Max(1.0, WorldMapSize.Y);
		const double MinX = WorldMapCenter.X - SizeX * 0.5;
		const double MinY = WorldMapCenter.Y - SizeY * 0.5;
		return FVector2D(
			MinX + (1.0 - UV.Y) * SizeX,
			MinY + UV.X * SizeY);
	}

	bool ContainsWorldXY(const FVector2D& WorldXY) const
	{
		const FVector2D HalfSize = WorldMapSize * 0.5;
		return WorldXY.X >= WorldMapCenter.X - HalfSize.X
			&& WorldXY.X <= WorldMapCenter.X + HalfSize.X
			&& WorldXY.Y >= WorldMapCenter.Y - HalfSize.Y
			&& WorldXY.Y <= WorldMapCenter.Y + HalfSize.Y;
	}
};
