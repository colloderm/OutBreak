// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OBWorldMapWidget.generated.h"

class UOverlay;
class UCanvasPanel;
class UImage;
class UTexture2D;
class UOBExpeditionMapData;

/**
왜 존재하는가?
 - M키 전체 지도. 배경은 미리 구운 탑다운 텍스처, 그 위에 마커만 얹는다.
무엇을 저장하는가?
 - 마커 위젯 풀만. 좌표는 매번 GameState/PlayerState에서 읽는다.
멀티플레이 역할?
 - 순수 표시. 개인 탈출구·팀원 위치는 소유자 전용으로 복제되어 온다.
 */
UCLASS()
class OUTBREAK_API UOBWorldMapWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// HUD가 M키 토글에 사용. 열려 있을 때만 갱신 타이머가 돈다.
	void SetMapOpen(bool bOpen);
	bool IsMapOpen() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	void Refresh();

	// 풀에서 하나 꺼내(없으면 생성) UV 위치에 놓는다. InOutIndex는 사용한 개수.
	void PlaceMarker(int32& InOutIndex, UTexture2D* Icon, const FVector2D& UV, const FLinearColor& Tint, float AngleDegrees = 0.f);

	const UOBExpeditionMapData* GetMapData() const;
	
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// 확대/이동을 공통 부모에 한 번에 적용. 마커는 따로 안 건드려도 따라온다.
	void ApplyViewTransform();
	void ResetView();
	bool TrySelectInsertionPoint(const FVector2D& ScreenPosition);

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> IMG_Map;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCanvasPanel> CVS_Markers;
	
	// IMG_Map과 CVS_Markers의 공통 부모. 여기에 확대/이동 트랜스폼을 건다.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UOverlay> OVR_MapRoot;

	// ===== 아이콘 =====
	UPROPERTY(EditDefaultsOnly, Category = "Map|Icons")
	TObjectPtr<UTexture2D> SelfIcon;

	UPROPERTY(EditDefaultsOnly, Category = "Map|Icons")
	TObjectPtr<UTexture2D> TeammateIcon;

	UPROPERTY(EditDefaultsOnly, Category = "Map|Icons")
	TObjectPtr<UTexture2D> PersonalExtractIcon;

	UPROPERTY(EditDefaultsOnly, Category = "Map|Icons")
	TObjectPtr<UTexture2D> PublicExtractIcon;

	UPROPERTY(EditDefaultsOnly, Category = "Map|Icons")
	TObjectPtr<UTexture2D> InsertionTargetIcon;

	UPROPERTY(EditDefaultsOnly, Category = "Map|Icons")
	FVector2D MarkerSize = FVector2D(28.f, 28.f);

	UPROPERTY(EditDefaultsOnly, Category = "Map|Icons")
	FLinearColor SelfColor = FLinearColor(1.f, 1.f, 1.f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "Map|Icons")
	FLinearColor TeammateColor = FLinearColor(0.2f, 0.8f, 1.f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "Map|Icons")
	FLinearColor PersonalExtractColor = FLinearColor(1.f, 0.85f, 0.1f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "Map|Icons")
	FLinearColor PublicExtractColor = FLinearColor(0.3f, 1.f, 0.3f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "Map|Icons")
	FLinearColor InsertionTargetColor = FLinearColor(1.f, 0.35f, 0.1f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "Map", Meta = (ClampMin = "0.05"))
	float RefreshInterval = 0.2f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Map|View", Meta = (ClampMin = "1.0"))
	float MinZoom = 1.f;

	UPROPERTY(EditDefaultsOnly, Category = "Map|View", Meta = (ClampMin = "1.0"))
	float MaxZoom = 8.f;

	// 휠 한 칸당 배율.
	UPROPERTY(EditDefaultsOnly, Category = "Map|View", Meta = (ClampMin = "1.01"))
	float ZoomStep = 1.25f;

	UPROPERTY(EditDefaultsOnly, Category = "Map|View", Meta = (ClampMin = "1.0"))
	float ClickDragThreshold = 6.f;

private:
	UPROPERTY()
	TArray<TObjectPtr<UImage>> MarkerPool;

	FTimerHandle RefreshTimer;
	
	// 0.2초 타이머라 경고를 한 번만 찍기 위한 플래그.
	bool bLoggedMapMissing = false;
	bool bLoggedTextureMissing = false;
	
	// 지도 좌표 보정용. 열 때 한 번만 찍는다. 맞추고 나면 지운다.
	bool bLogCalibration = false;
	
	float ZoomLevel = 1.f;
	FVector2D PanOffset = FVector2D::ZeroVector;

	bool bDragging = false;
	bool bDragThresholdExceeded = false;
	FVector2D MouseDownScreenPos = FVector2D::ZeroVector;
	FVector2D LastDragScreenPos = FVector2D::ZeroVector;
	
};
