#include "InteriorPCGWallVolumeDetails.h"

#include "InteriorPCGWallVolume.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Framework/Notifications/NotificationManager.h"
#include "ScopedTransaction.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "InteriorPCGWallVolumeDetails"

namespace InteriorPCGWallVolumeDetailsPrivate
{
	void Notify(const FText& Message)
	{
		FNotificationInfo Info(Message);
		Info.ExpireDuration = 4.0f;
		Info.bUseLargeFont = false;
		if (TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
		{
			Item->SetCompletionState(SNotificationItem::CS_Success);
		}
	}

	void ModifyGeneratorAndLevel(AInteriorPCGWallVolume* Generator)
	{
		Generator->Modify();
		if (ULevel* Level = Generator->GetLevel())
		{
			Level->Modify();
		}
	}
}

TSharedRef<IDetailCustomization> FInteriorPCGWallVolumeDetails::MakeInstance()
{
	return MakeShared<FInteriorPCGWallVolumeDetails>();
}

void FInteriorPCGWallVolumeDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	FInteriorPCGVolumeDetails::CustomizeDetails(DetailBuilder);

	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);
	if (!Objects.IsEmpty())
	{
		WallVolume = Cast<AInteriorPCGWallVolume>(Objects[0].Get());
	}

	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(
		"Interior Wall PCG Actions",
		LOCTEXT("WallActionsCategory", "내부 벽 PCG 작업"),
		ECategoryPriority::Important);

	Category.AddCustomRow(LOCTEXT("WallGenerationActions", "내부 벽 생성"))
	.WholeRowContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.0f)
		[
			SNew(SButton)
			.Text(LOCTEXT("GenerateWalls", "벽·문·계단 생성"))
			.ToolTipText(LOCTEXT("GenerateWallsTip", "Wall Seed와 등록한 벽/문/계단 에셋만 사용해 구조 Actor를 다시 생성합니다. 기존 가구는 유지됩니다."))
			.OnClicked(this, &FInteriorPCGWallVolumeDetails::OnGenerateWalls)
		]
		+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.0f)
		[
			SNew(SButton)
			.Text(LOCTEXT("GenerateAll", "가구 + 내부 벽 생성"))
			.ToolTipText(LOCTEXT("GenerateAllTip", "기존 Preview를 정리하고 가구를 먼저 생성한 뒤, 가구 충돌을 피해서 내부 벽을 생성합니다."))
			.OnClicked(this, &FInteriorPCGWallVolumeDetails::OnGenerateWallsAndInterior)
		]
		+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.0f)
		[
			SNew(SButton)
			.Text(LOCTEXT("ClearWalls", "벽·문·계단 정리"))
			.ToolTipText(LOCTEXT("ClearWallsTip", "이 Generator가 만든 내부 벽, 문 벽, 계단만 삭제하고 가구/Prop은 유지합니다."))
			.OnClicked(this, &FInteriorPCGWallVolumeDetails::OnClearWalls)
		]
	];
}

FReply FInteriorPCGWallVolumeDetails::OnGenerateWalls()
{
	if (AInteriorPCGWallVolume* Generator = WallVolume.Get())
	{
		const FScopedTransaction Transaction(LOCTEXT("GenerateWallsTransaction", "내부 PCG 벽 생성"));
		InteriorPCGWallVolumeDetailsPrivate::ModifyGeneratorAndLevel(Generator);
		const int32 Count = Generator->GenerateInteriorWalls();
		InteriorPCGWallVolumeDetailsPrivate::Notify(FText::Format(LOCTEXT("WallsGenerated", "편집 가능한 벽/문/계단 Actor {0}개를 생성했습니다."), Count));
	}
	return FReply::Handled();
}

FReply FInteriorPCGWallVolumeDetails::OnGenerateWallsAndInterior()
{
	if (AInteriorPCGWallVolume* Generator = WallVolume.Get())
	{
		const FScopedTransaction Transaction(LOCTEXT("GenerateAllTransaction", "가구와 내부 PCG 벽 생성"));
		InteriorPCGWallVolumeDetailsPrivate::ModifyGeneratorAndLevel(Generator);
		const int32 Count = Generator->GenerateWallsAndInterior();
		InteriorPCGWallVolumeDetailsPrivate::Notify(FText::Format(LOCTEXT("AllGenerated", "편집 가능한 가구/벽 Actor {0}개를 생성했습니다."), Count));
	}
	return FReply::Handled();
}

FReply FInteriorPCGWallVolumeDetails::OnClearWalls()
{
	if (AInteriorPCGWallVolume* Generator = WallVolume.Get())
	{
		const FScopedTransaction Transaction(LOCTEXT("ClearWallsTransaction", "내부 PCG 벽 정리"));
		InteriorPCGWallVolumeDetailsPrivate::ModifyGeneratorAndLevel(Generator);
		const int32 Count = Generator->ClearGeneratedWalls();
		InteriorPCGWallVolumeDetailsPrivate::Notify(FText::Format(LOCTEXT("WallsCleared", "벽/문/계단 Actor {0}개를 정리했습니다."), Count));
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
