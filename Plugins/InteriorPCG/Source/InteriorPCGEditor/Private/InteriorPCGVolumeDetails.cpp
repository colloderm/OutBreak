#include "InteriorPCGVolumeDetails.h"

#include "InteriorPCGPreset.h"
#include "InteriorPCGPresetFactory.h"
#include "InteriorPCGVolume.h"

#include "AssetToolsModule.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "FileHelpers.h"
#include "Framework/Notifications/NotificationManager.h"
#include "IAssetTools.h"
#include "ScopedTransaction.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "InteriorPCGVolumeDetails"

namespace InteriorPCGVolumeDetailsPrivate
{
	void Notify(const FText& Message, const SNotificationItem::ECompletionState State = SNotificationItem::CS_Success)
	{
		FNotificationInfo Info(Message);
		Info.ExpireDuration = 4.0f;
		Info.bUseLargeFont = false;
		if (TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
		{
			Item->SetCompletionState(State);
		}
	}

	bool SavePresetPackage(UInteriorPCGPreset* Preset)
	{
		if (!Preset)
		{
			return false;
		}

		TArray<UPackage*> PackagesToSave{ Preset->GetOutermost() };
		TArray<UPackage*> FailedPackages;
		return FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, false, false, &FailedPackages, false, false) == FEditorFileUtils::PR_Success;
	}
}

TSharedRef<IDetailCustomization> FInteriorPCGVolumeDetails::MakeInstance()
{
	return MakeShared<FInteriorPCGVolumeDetails>();
}

void FInteriorPCGVolumeDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);
	if (Objects.Num() > 0)
	{
		Volume = Cast<AInteriorPCGVolume>(Objects[0].Get());
	}

	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory("Interior PCG Actions", LOCTEXT("ActionsCategory", "Interior PCG Actions"), ECategoryPriority::Important);
	Category.AddCustomRow(LOCTEXT("GenerationActions", "Generate Interior"))
	.WholeRowContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.0f)
		[
			SNew(SButton)
			.Text(LOCTEXT("GenerateRandom", "Generate Random Interior"))
			.ToolTipText(LOCTEXT("GenerateRandomTip", "Clear registered preview actors and generate a deterministic layout from the explicit asset entries."))
			.OnClicked(this, &FInteriorPCGVolumeDetails::OnGenerateRandom)
		]
		+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.0f)
		[
			SNew(SButton)
			.Text(LOCTEXT("GeneratePreset", "Generate Selected Preset"))
			.ToolTipText(LOCTEXT("GeneratePresetTip", "Clear registered preview actors and rebuild the selected PCG Preset against this volume's floor."))
			.OnClicked(this, &FInteriorPCGVolumeDetails::OnGeneratePreset)
		]
	];

	Category.AddCustomRow(LOCTEXT("PresetActions", "Save PCG Preset"))
	.WholeRowContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.0f)
		[
			SNew(SButton)
			.Text(LOCTEXT("SaveNewPreset", "Save As New PCG Preset"))
			.OnClicked(this, &FInteriorPCGVolumeDetails::OnSaveNewPreset)
		]
		+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.0f)
		[
			SNew(SButton)
			.Text(LOCTEXT("UpdatePreset", "Update Selected Preset"))
			.OnClicked(this, &FInteriorPCGVolumeDetails::OnUpdatePreset)
		]
	];

	Category.AddCustomRow(LOCTEXT("EditActions", "Edit Interior"))
	.WholeRowContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.0f)
		[
			SNew(SButton)
			.Text(LOCTEXT("RegisterActors", "Register Listed / Selected Props"))
			.ToolTipText(LOCTEXT("RegisterActorsTip", "Register actors in Props To Register plus any currently selected level actors as editable props owned by this generator."))
			.OnClicked(this, &FInteriorPCGVolumeDetails::OnRegisterSelectedActors)
		]
		+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.0f)
		[
			SNew(SButton)
			.Text(LOCTEXT("ClearGenerated", "Clear Preview"))
			.OnClicked(this, &FInteriorPCGVolumeDetails::OnClearGenerated)
		]
	];
}

FReply FInteriorPCGVolumeDetails::OnGenerateRandom()
{
	if (AInteriorPCGVolume* Generator = Volume.Get())
	{
		const FScopedTransaction Transaction(LOCTEXT("GenerateRandomTransaction", "Generate Random Interior"));
		Generator->Modify();
		if (ULevel* Level = Generator->GetLevel())
		{
			Level->Modify();
		}
		const int32 Count = Generator->GenerateRandomInterior();
		InteriorPCGVolumeDetailsPrivate::Notify(FText::Format(LOCTEXT("GeneratedCount", "Generated {0} editable actors."), Count));
	}
	return FReply::Handled();
}

FReply FInteriorPCGVolumeDetails::OnGeneratePreset()
{
	if (AInteriorPCGVolume* Generator = Volume.Get())
	{
		if (!Generator->SelectedPreset)
		{
			InteriorPCGVolumeDetailsPrivate::Notify(LOCTEXT("NoPreset", "Select a PCG Preset first."), SNotificationItem::CS_Fail);
			return FReply::Handled();
		}

		const FScopedTransaction Transaction(LOCTEXT("GeneratePresetTransaction", "Generate Interior From Preset"));
		Generator->Modify();
		if (ULevel* Level = Generator->GetLevel())
		{
			Level->Modify();
		}
		const int32 Count = Generator->GenerateFromSelectedPreset();
		InteriorPCGVolumeDetailsPrivate::Notify(FText::Format(LOCTEXT("PresetGeneratedCount", "Generated {0} editable actors from the preset."), Count));
	}
	return FReply::Handled();
}

FReply FInteriorPCGVolumeDetails::OnSaveNewPreset()
{
	AInteriorPCGVolume* Generator = Volume.Get();
	if (!Generator)
	{
		return FReply::Handled();
	}

	UInteriorPCGPresetFactory* Factory = NewObject<UInteriorPCGPresetFactory>();
	UInteriorPCGPreset* Preset = Cast<UInteriorPCGPreset>(FAssetToolsModule::GetModule().Get().CreateAssetWithDialog(
		TEXT("PCGPreset_Interior"),
		TEXT("/Game/InteriorPCG/Presets"),
		UInteriorPCGPreset::StaticClass(),
		Factory,
		FName(TEXT("InteriorPCGSavePreset")),
		false));
	if (!Preset)
	{
		return FReply::Handled();
	}

	const FScopedTransaction Transaction(LOCTEXT("SaveNewPresetTransaction", "Save New Interior PCG Preset"));
	Generator->Modify();
	Preset->Modify();
	const int32 Count = Generator->CaptureCurrentPlacement(Preset);
	Generator->SelectedPreset = Preset;
	Preset->MarkPackageDirty();
	const bool bSaved = InteriorPCGVolumeDetailsPrivate::SavePresetPackage(Preset);
	TArray<UObject*> ObjectsToSync{ Preset };
	GEditor->SyncBrowserToObjects(ObjectsToSync);
	InteriorPCGVolumeDetailsPrivate::Notify(
		bSaved
			? FText::Format(LOCTEXT("SavedPreset", "Saved PCG Preset with {0} placement records."), Count)
			: LOCTEXT("SavePresetFailed", "The preset was created but its package could not be saved."),
		bSaved ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
	return FReply::Handled();
}

FReply FInteriorPCGVolumeDetails::OnUpdatePreset()
{
	AInteriorPCGVolume* Generator = Volume.Get();
	if (!Generator || !Generator->SelectedPreset)
	{
		InteriorPCGVolumeDetailsPrivate::Notify(LOCTEXT("NoPresetToUpdate", "Select a PCG Preset to update."), SNotificationItem::CS_Fail);
		return FReply::Handled();
	}

	const FScopedTransaction Transaction(LOCTEXT("UpdatePresetTransaction", "Update Interior PCG Preset"));
	Generator->Modify();
	Generator->SelectedPreset->Modify();
	const int32 Count = Generator->CaptureCurrentPlacement(Generator->SelectedPreset);
	const bool bSaved = InteriorPCGVolumeDetailsPrivate::SavePresetPackage(Generator->SelectedPreset);
	InteriorPCGVolumeDetailsPrivate::Notify(
		bSaved
			? FText::Format(LOCTEXT("UpdatedPreset", "Updated PCG Preset with {0} placement records."), Count)
			: LOCTEXT("UpdatePresetFailed", "The preset was updated in memory but its package could not be saved."),
		bSaved ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
	return FReply::Handled();
}

FReply FInteriorPCGVolumeDetails::OnClearGenerated()
{
	if (AInteriorPCGVolume* Generator = Volume.Get())
	{
		const FScopedTransaction Transaction(LOCTEXT("ClearTransaction", "Clear Interior PCG Preview"));
		Generator->Modify();
		if (ULevel* Level = Generator->GetLevel())
		{
			Level->Modify();
		}
		const int32 Count = Generator->ClearGeneratedActors();
		InteriorPCGVolumeDetailsPrivate::Notify(FText::Format(LOCTEXT("ClearedCount", "Removed {0} registered preview actors."), Count));
	}
	return FReply::Handled();
}

FReply FInteriorPCGVolumeDetails::OnRegisterSelectedActors()
{
	AInteriorPCGVolume* Generator = Volume.Get();
	if (!Generator || !GEditor)
	{
		return FReply::Handled();
	}

	const FScopedTransaction Transaction(LOCTEXT("RegisterTransaction", "Register Interior PCG Props"));
	Generator->Modify();
	int32 RegisteredCount = 0;
	TSet<AActor*> ActorsToRegister;
	for (AActor* Actor : Generator->PropsToRegister)
	{
		if (Actor)
		{
			ActorsToRegister.Add(Actor);
		}
	}
	for (FSelectionIterator It(*GEditor->GetSelectedActors()); It; ++It)
	{
		if (AActor* Actor = Cast<AActor>(*It))
		{
			ActorsToRegister.Add(Actor);
		}
	}
	for (AActor* Actor : ActorsToRegister)
	{
		RegisteredCount += Generator->RegisterActor(Actor) ? 1 : 0;
	}
	Generator->PropsToRegister.Reset();

	InteriorPCGVolumeDetailsPrivate::Notify(FText::Format(LOCTEXT("RegisteredCount", "Registered {0} selected props."), RegisteredCount));
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
