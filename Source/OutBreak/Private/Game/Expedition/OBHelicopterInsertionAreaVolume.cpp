#include "Game/Expedition/OBHelicopterInsertionAreaVolume.h"

#include "Components/BrushComponent.h"
#include "Core/OBCollisionChannels.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogOBInsertionAreaVolume, Log, All);

AOBHelicopterInsertionAreaVolume::AOBHelicopterInsertionAreaVolume(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ConfigureLogicalVolumeCollision();
}

void AOBHelicopterInsertionAreaVolume::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Serialized level/component values and Blueprint construction can restore the
	// inherited AVolume collision profile. Enforce the logical-volume contract
	// after components and instance properties have been initialized.
	ConfigureLogicalVolumeCollision();
}

void AOBHelicopterInsertionAreaVolume::ConfigureLogicalVolumeCollision()
{
	UBrushComponent* VolumeBrushComponent = GetBrushComponent();
	if (!VolumeBrushComponent)
	{
		return;
	}

	// QueryOnly preserves the brush physics shape required by AVolume::EncompassesPoint.
	// Ignoring every channel prevents this allow-list volume from becoming an
	// invisible wall for camera probes, weapon traces, pawns, or visibility tests.
	VolumeBrushComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	VolumeBrushComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	VolumeBrushComponent->SetGenerateOverlapEvents(false);
	VolumeBrushComponent->SetCanEverAffectNavigation(false);

	ensureAlwaysMsgf(
		VolumeBrushComponent->GetCollisionResponseToChannel(OB_TraceChannel_Weapon) == ECR_Ignore
			&& VolumeBrushComponent->GetCollisionResponseToChannel(OB_TraceChannel_CameraProbe) == ECR_Ignore,
		TEXT("Insertion area volume %s must ignore Weapon and CameraProbe traces."),
		*GetName());

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		UE_LOG(LogOBInsertionAreaVolume, Log,
			TEXT("[InsertionArea] Logical collision enforced Volume=%s Collision=%s Weapon=%d CameraProbe=%d"),
			*GetName(), *UEnum::GetValueAsString(VolumeBrushComponent->GetCollisionEnabled()),
			static_cast<int32>(VolumeBrushComponent->GetCollisionResponseToChannel(OB_TraceChannel_Weapon)),
			static_cast<int32>(VolumeBrushComponent->GetCollisionResponseToChannel(OB_TraceChannel_CameraProbe)));
	}
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOBInsertionAreaVolumeCollisionContractTest,
	"OutBreak.Expedition.LogicalVolumes.InsertionAreaCollisionContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOBInsertionAreaVolumeCollisionContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const AOBHelicopterInsertionAreaVolume* VolumeCDO = GetDefault<AOBHelicopterInsertionAreaVolume>();
	const UBrushComponent* VolumeBrushComponent = VolumeCDO ? VolumeCDO->GetBrushComponent() : nullptr;
	if (!TestNotNull(TEXT("Insertion area CDO has a brush component"), VolumeBrushComponent))
	{
		return false;
	}

	TestEqual(TEXT("Insertion area keeps a query shape"),
		VolumeBrushComponent->GetCollisionEnabled(), ECollisionEnabled::QueryOnly);
	TestEqual(TEXT("Insertion area ignores Weapon traces"),
		VolumeBrushComponent->GetCollisionResponseToChannel(OB_TraceChannel_Weapon), ECR_Ignore);
	TestEqual(TEXT("Insertion area ignores CameraProbe traces"),
		VolumeBrushComponent->GetCollisionResponseToChannel(OB_TraceChannel_CameraProbe), ECR_Ignore);
	TestFalse(TEXT("Insertion area does not generate overlaps"),
		VolumeBrushComponent->GetGenerateOverlapEvents());
	return true;
}
#endif
