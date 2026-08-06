// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Widgets/Expedition/OBWorldMapWidget.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Game/Expedition/OBExpeditionMapData.h"
#include "Game/GameState/OBExpeditionGameState.h"
#include "Player/State/OBPlayerStateBase.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Components/Overlay.h"


void UOBWorldMapWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// HUD가 상시 생성해 두고 숨긴다. 열기 전에는 타이머를 돌리지 않는다.
	SetVisibility(ESlateVisibility::Collapsed);
}

void UOBWorldMapWidget::NativeDestruct()
{
	GetWorld()->GetTimerManager().ClearTimer(RefreshTimer);
	
	Super::NativeDestruct();
}

bool UOBWorldMapWidget::IsMapOpen() const
{
	return GetVisibility() != ESlateVisibility::Collapsed;
}

void UOBWorldMapWidget::SetMapOpen(bool bOpen)
{
	// 휠/드래그를 받아야 하므로 Visible이어야 한다(HitTestInvisible은 입력을 못 받는다).
	SetVisibility(bOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	if (APlayerController* PC = GetOwningPlayer())
	{
		if (bOpen)
		{
			// GameAndUI: WASD 이동은 캐릭터로 계속 가고, 마우스만 지도가 가져간다.
			// 지도를 보는 동안 시점 회전이 멈추는 건 의도된 대가다.
			FInputModeGameAndUI Mode;
			Mode.SetHideCursorDuringCapture(false);
			Mode.SetLockMouseToViewportBehavior(EMouseLockMode::LockOnCapture);
			PC->SetInputMode(Mode);
			PC->SetShowMouseCursor(true);
		}
		else
		{
			PC->SetInputMode(FInputModeGameOnly());
			PC->SetShowMouseCursor(false);
		}
	}

	FTimerManager& Timers = GetWorld()->GetTimerManager();
	if (bOpen)
	{
		bLoggedMapMissing = false;
		bLoggedTextureMissing = false;
		bLogCalibration = true;

		ResetView();   // 열 때마다 전체 보기로 시작한다.
		Refresh();
		Timers.SetTimer(RefreshTimer, this, &UOBWorldMapWidget::Refresh, RefreshInterval, true);
	}
	else
	{
		bDragging = false;
		Timers.ClearTimer(RefreshTimer);
	}
}

const UOBExpeditionMapData* UOBWorldMapWidget::GetMapData() const
{
	const UWorld* W = GetWorld();
	const AOBExpeditionGameState* GS = W ? W->GetGameState<AOBExpeditionGameState>() : nullptr;
	
	return GS ? GS->GetMapData() : nullptr;
}

void UOBWorldMapWidget::Refresh()
{	
	const UOBExpeditionMapData* Map = GetMapData();
	if (!Map)
	{
		// 0.2초마다 도는 함수라 한 번만 찍는다.
		if (!bLoggedMapMissing)
		{
			bLoggedMapMissing = true;
			UE_LOG(LogTemp, Warning,
				TEXT("[Map] GameState의 MapData가 비었다. GameMode의 Active Map Data 지정과 StartPlay의 SetMapData 호출을 확인할 것."));
		}
		return;
	}

	if (!Map->WorldMapTexture)
	{
		if (!bLoggedTextureMissing)
		{
			bLoggedTextureMissing = true;
			UE_LOG(LogTemp, Warning, TEXT("[Map] %s 의 World Map Texture가 비었다. 구운 탑다운 텍스처를 지정할 것."), *Map->GetName());
		}
	}
	else if (IMG_Map)
	{
		IMG_Map->SetBrushFromTexture(Map->WorldMapTexture);
	}

	int32 Index = 0;

	const AOBExpeditionGameState* GS = GetWorld()->GetGameState<AOBExpeditionGameState>();
	const AOBPlayerStateBase* MyPS = GetOwningPlayerState<AOBPlayerStateBase>();

	// 그리는 순서 = 겹칠 때 아래에서 위. 내 아이콘이 항상 맨 위여야 한다.
	if (GS)
	{
		for (const FVector_NetQuantize& Loc : GS->GetPublicExtractLocations())
		{
			PlaceMarker(Index, PublicExtractIcon, Map->WorldToMapUV(Loc), PublicExtractColor);
		}
	}

	if (MyPS)
	{
		for (const FVector_NetQuantize& Loc : MyPS->GetTeammateMapLocations())
		{
			const FVector2D UV = Map->WorldToMapUV(Loc);
			PlaceMarker(Index, TeammateIcon, UV, TeammateColor);
		}

		for (const FVector_NetQuantize& Loc : MyPS->GetTeammateMapLocations())
		{
			PlaceMarker(Index, TeammateIcon, Map->WorldToMapUV(Loc), TeammateColor);
		}
	}

	if (const APawn* MyPawn = GetOwningPlayerPawn())
	{
		const FVector MyLoc = MyPawn->GetActorLocation();
		const FVector2D MyUV = Map->WorldToMapUV(MyLoc);

#if !UE_BUILD_SHIPPING
		if (bLogCalibration)
		{
			bLogCalibration = false;
			UE_LOG(LogTemp, Log,
				TEXT("[Map] 내 위치 World(%.0f, %.0f) → UV(%.3f, %.3f) | Center(%.0f, %.0f) Size(%.0f, %.0f)"),
				MyLoc.X, MyLoc.Y, MyUV.X, MyUV.Y,
				Map->WorldMapCenter.X, Map->WorldMapCenter.Y, Map->WorldMapSize.X, Map->WorldMapSize.Y);
		}
#endif

		// 이미지 위쪽 = 월드 +X이므로 Yaw를 그대로 회전각으로 쓴다.
		PlaceMarker(Index, SelfIcon, MyUV, SelfColor, MyPawn->GetActorRotation().Yaw);
	}
}

void UOBWorldMapWidget::PlaceMarker(int32& InOutIndex, UTexture2D* Icon, const FVector2D& UV, const FLinearColor& Tint, float AngleDegrees)
{
	if (!Icon || !CVS_Markers) return;

	// 캡처 범위를 벗어난 좌표는 그리지 않는다(지도 밖으로 삐져나가지 않게).
	if (UV.X < 0.f || UV.X > 1.f || UV.Y < 0.f || UV.Y > 1.f) return;

	UImage* Marker = MarkerPool.IsValidIndex(InOutIndex) ? MarkerPool[InOutIndex].Get() : nullptr;
	if (!Marker)
	{
		Marker = NewObject<UImage>(this);
		MarkerPool.Add(Marker);
		CVS_Markers->AddChild(Marker);
	}

	// 앵커를 UV에 맞추면 지도 위젯 크기가 바뀌어도 마커가 따라간다.
	if (UCanvasPanelSlot* CPSlot = Cast<UCanvasPanelSlot>(Marker->Slot))
	{
		CPSlot->SetAnchors(FAnchors(UV.X, UV.Y, UV.X, UV.Y));
		CPSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		CPSlot->SetAutoSize(false);
		CPSlot->SetOffsets(FMargin(0.f, 0.f, MarkerSize.X, MarkerSize.Y));
	}

	Marker->SetBrushFromTexture(Icon);
	Marker->SetDesiredSizeOverride(MarkerSize);
	Marker->SetColorAndOpacity(Tint);
	Marker->SetRenderTransformAngle(AngleDegrees);
	Marker->SetVisibility(ESlateVisibility::HitTestInvisible);

	++InOutIndex;
}

void UOBWorldMapWidget::ResetView()
{
	ZoomLevel = MinZoom;
	PanOffset = FVector2D::ZeroVector;
	ApplyViewTransform();
}

void UOBWorldMapWidget::ApplyViewTransform()
{
	if (!OVR_MapRoot) return;

	// 이동 한계: 배율 1이면 0(항상 중앙), 확대할수록 넉넉해진다.
	const FVector2D Size = OVR_MapRoot->GetCachedGeometry().GetLocalSize();
	const FVector2D MaxPan = Size * 0.5f * FMath::Max(0.f, ZoomLevel - 1.f);

	PanOffset.X = FMath::Clamp(PanOffset.X, -MaxPan.X, MaxPan.X);
	PanOffset.Y = FMath::Clamp(PanOffset.Y, -MaxPan.Y, MaxPan.Y);

	OVR_MapRoot->SetRenderScale(FVector2D(ZoomLevel, ZoomLevel));
	OVR_MapRoot->SetRenderTranslation(PanOffset);
}

FReply UOBWorldMapWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const float Delta = InMouseEvent.GetWheelDelta();
	if (FMath::IsNearlyZero(Delta)) return FReply::Unhandled();

	const float OldZoom = ZoomLevel;
	ZoomLevel = FMath::Clamp(
		OldZoom * FMath::Pow(ZoomStep, Delta), MinZoom, MaxZoom);

	if (FMath::IsNearlyEqual(OldZoom, ZoomLevel))
	{
		return FReply::Handled();   // 한계에 닿음. 확대는 소비하되 아무 일도 안 한다.
	}

	// 커서가 가리키던 지점이 확대 후에도 같은 자리에 남게 한다.
	// (중앙 기준으로만 확대하면 큰 맵에서 원하는 곳을 못 본다.)
	const FVector2D LocalCursor =
		InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition())
		- InGeometry.GetLocalSize() * 0.5f;

	PanOffset = LocalCursor - (LocalCursor - PanOffset) * (ZoomLevel / OldZoom);

	ApplyViewTransform();
	return FReply::Handled();
}

FReply UOBWorldMapWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}

	bDragging = true;
	LastDragScreenPos = InMouseEvent.GetScreenSpacePosition();

	// 캡처해야 위젯 밖으로 나가도 드래그가 이어진다.
	return FReply::Handled().CaptureMouse(TakeWidget());
}

FReply UOBWorldMapWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bDragging) return FReply::Unhandled();

	bDragging = false;
	return FReply::Handled().ReleaseMouseCapture();
}

FReply UOBWorldMapWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bDragging) return FReply::Unhandled();

	const FVector2D Now = InMouseEvent.GetScreenSpacePosition();
	PanOffset += (Now - LastDragScreenPos);
	LastDragScreenPos = Now;

	ApplyViewTransform();
	return FReply::Handled();
}
