// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Widgets/Expedition/OBWorldMapWidget.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Game/Expedition/OBExpeditionMapData.h"
#include "Game/GameState/OBExpeditionGameState.h"
#include "Player/State/OBPlayerStateBase.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Input/Events.h"
#include "Player/Controller/OBPlayerController.h"
#include "TimerManager.h"
#include "Components/Overlay.h"

DEFINE_LOG_CATEGORY_STATIC(LogOBInsertionMap, Log, All);

void UOBWorldMapWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// HUD가 상시 생성해 두고 숨긴다. 열기 전에는 타이머를 돌리지 않는다.
	SetVisibility(ESlateVisibility::Collapsed);
}

void UOBWorldMapWidget::NativeDestruct()
{
	//GetWorld()->GetTimerManager().ClearTimer(RefreshTimer);
	// 레벨 전환/PIE 종료 중에는 GetWorld()가 null이 될 수 있다.
	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().ClearTimer(RefreshTimer);
	}
	Super::NativeDestruct();
}

FReply UOBWorldMapWidget::NativeOnPreviewKeyDown(
	const FGeometry& InGeometry,
	const FKeyEvent& InKeyEvent)
{
	if (bInsertionSelectionMode)
	{
		if (AOBPlayerController* PC = Cast<AOBPlayerController>(GetOwningPlayer()))
		{
			const FKey Key = InKeyEvent.GetKey();
			if (Key == EKeys::M)
			{
				UE_LOG(LogOBInsertionMap, Display,
					TEXT("[InsertionUI] Preview key received Key=M Widget=%s PC=%s"),
					*GetName(), *PC->GetName());
				PC->ToggleInsertionMapFromFocusedWidget();
				return FReply::Handled();
			}
			if (Key == EKeys::E)
			{
				UE_LOG(LogOBInsertionMap, Display,
					TEXT("[InsertionUI] Preview key received Key=E Widget=%s PC=%s"),
					*GetName(), *PC->GetName());
				PC->RequestInsertionPointFromFocusedWidget();
				return FReply::Handled();
			}
		}
	}

	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
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
			SetIsFocusable(true);
			Mode.SetWidgetToFocus(TakeWidget());
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

	UE_LOG(LogOBInsertionMap, Log,
		TEXT("[InsertionUI] Map visibility changed Widget=%s Open=%s SelectionMode=%s CanSelect=%s Owner=%s"),
		*GetName(), bOpen ? TEXT("true") : TEXT("false"),
		bInsertionSelectionMode ? TEXT("true") : TEXT("false"),
		bCanSelectInsertionTarget ? TEXT("true") : TEXT("false"),
		*GetNameSafe(GetOwningPlayer()));

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

void UOBWorldMapWidget::SetInsertionSelectionMode(bool bEnabled, bool bCanSelectTarget)
{
	bInsertionSelectionMode = bEnabled;
	bCanSelectInsertionTarget = bEnabled && bCanSelectTarget;
	UE_LOG(LogOBInsertionMap, Log,
		TEXT("[InsertionUI] Selection mode Widget=%s Enabled=%s CanSelect=%s"),
		*GetName(), bInsertionSelectionMode ? TEXT("true") : TEXT("false"),
		bCanSelectInsertionTarget ? TEXT("true") : TEXT("false"));
	BP_OnInsertionSelectionModeChanged(bInsertionSelectionMode, bCanSelectInsertionTarget);
}

void UOBWorldMapWidget::NotifyInsertionPointResult(
	bool bAccepted,
	const FVector& ResolvedLocation,
	const FString& Message)
{
	if (bAccepted)
	{
		UE_LOG(LogOBInsertionMap, Log,
			TEXT("[InsertionUI] Selection result Accepted=true Resolved=%s Message=%s"),
			*ResolvedLocation.ToCompactString(), *Message);
	}
	else
	{
		UE_LOG(LogOBInsertionMap, Warning,
			TEXT("[InsertionUI] Selection result Accepted=false Resolved=%s Message=%s"),
			*ResolvedLocation.ToCompactString(), *Message);
	}
	BP_OnInsertionPointResult(bAccepted, ResolvedLocation, Message);
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


		if (GS)
		{
			FOBTeamInsertionState InsertionState;
			if (GS->GetTeamInsertionState(MyPS->GetTeamId(), InsertionState))
			{
				if (InsertionState.bHasResolvedLocation)
				{
					PlaceMarker(Index, InsertionTargetIcon,
						Map->WorldToMapUV(InsertionState.ResolvedGroundLocation), InsertionTargetColor);
				}
				else if (InsertionState.bHasRequestedLocation)
				{
					PlaceMarker(Index, InsertionTargetIcon,
						Map->WorldToMapUV(InsertionState.RequestedLocation), InsertionTargetColor);
				}
			}
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
	bDragThresholdExceeded = false;
	MouseDownScreenPos = InMouseEvent.GetScreenSpacePosition();
	LastDragScreenPos = InMouseEvent.GetScreenSpacePosition();

	// 캡처해야 위젯 밖으로 나가도 드래그가 이어진다.
	return FReply::Handled().CaptureMouse(TakeWidget());
}

FReply UOBWorldMapWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bDragging) return FReply::Unhandled();

	const bool bWasDrag = bDragThresholdExceeded;
	bDragging = false;
	bDragThresholdExceeded = false;
	if (!bWasDrag)
	{
		TrySelectInsertionPoint(InMouseEvent.GetScreenSpacePosition());
	}
	return FReply::Handled().ReleaseMouseCapture();
}

FReply UOBWorldMapWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bDragging) return FReply::Unhandled();

	const FVector2D Now = InMouseEvent.GetScreenSpacePosition();
	if (!bDragThresholdExceeded)
	{
		bDragThresholdExceeded = FVector2D::Distance(Now, MouseDownScreenPos) >= ClickDragThreshold;
		if (!bDragThresholdExceeded)
		{
			return FReply::Handled();
		}
	}
	PanOffset += (Now - LastDragScreenPos);
	LastDragScreenPos = Now;

	ApplyViewTransform();
	return FReply::Handled();
}

bool UOBWorldMapWidget::TrySelectInsertionPoint(const FVector2D& ScreenPosition)
{
	const UOBExpeditionMapData* Map = GetMapData();
	const AOBExpeditionGameState* GS = GetWorld() ? GetWorld()->GetGameState<AOBExpeditionGameState>() : nullptr;
	const AOBPlayerStateBase* PS = GetOwningPlayerState<AOBPlayerStateBase>();
	AOBPlayerController* PC = Cast<AOBPlayerController>(GetOwningPlayer());
	if (!bInsertionSelectionMode || !bCanSelectInsertionTarget || !Map || !GS || !PS || !PC
		|| GS->GetPhase() != EOBExpeditionPhase::Insertion || !PS->IsPartyLeader() || !IMG_Map)
	{
		UE_LOG(LogOBInsertionMap, Warning,
			TEXT("[InsertionUI] Click rejected before coordinate conversion Mode=%s CanSelect=%s Map=%s GS=%s PS=%s PC=%s Phase=%d Leader=%s Image=%s"),
			bInsertionSelectionMode ? TEXT("true") : TEXT("false"),
			bCanSelectInsertionTarget ? TEXT("true") : TEXT("false"),
			*GetNameSafe(Map), *GetNameSafe(GS), *GetNameSafe(PS), *GetNameSafe(PC),
			GS ? static_cast<int32>(GS->GetPhase()) : -1,
			PS && PS->IsPartyLeader() ? TEXT("true") : TEXT("false"), *GetNameSafe(IMG_Map));
		return false;
	}

	FOBTeamInsertionState State;
	if (GS->GetTeamInsertionState(PS->GetTeamId(), State)
		&& State.Phase != EOBInsertionPhase::WaitingForTarget
		&& State.Phase != EOBInsertionPhase::Orbiting)
	{
		UE_LOG(LogOBInsertionMap, Warning,
			TEXT("[InsertionUI] Click rejected because Team=%d insertion phase=%d"),
			PS->GetTeamId(), static_cast<int32>(State.Phase));
		return false;
	}

	const FGeometry& MapGeometry = IMG_Map->GetCachedGeometry();
	const FVector2D MapSize = MapGeometry.GetLocalSize();
	if (MapSize.X <= 1.f || MapSize.Y <= 1.f)
	{
		UE_LOG(LogOBInsertionMap, Warning,
			TEXT("[InsertionUI] Click rejected because map geometry is invalid Size=%s"), *MapSize.ToString());
		return false;
	}

	const FVector2D Local = MapGeometry.AbsoluteToLocal(ScreenPosition);
	const FVector2D UV(Local.X / MapSize.X, Local.Y / MapSize.Y);
	if (UV.X < 0.f || UV.X > 1.f || UV.Y < 0.f || UV.Y > 1.f)
	{
		UE_LOG(LogOBInsertionMap, Verbose,
			TEXT("[InsertionUI] Click outside map Screen=%s Local=%s UV=%s"),
			*ScreenPosition.ToString(), *Local.ToString(), *UV.ToString());
		return false;
	}

	const FVector2D WorldXY = Map->MapUVToWorldXY(UV);
	UE_LOG(LogOBInsertionMap, Log,
		TEXT("[InsertionUI] Click submitted Screen=%s Local=%s UV=%s WorldXY=%s Team=%d PC=%s"),
		*ScreenPosition.ToString(), *Local.ToString(), *UV.ToString(), *WorldXY.ToString(),
		PS->GetTeamId(), *PC->GetName());
	PC->RequestInsertionPoint(WorldXY);
	return true;
}
