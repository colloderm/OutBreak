#include "Game/Expedition/OBHelicopterExclusionVolume.h"

#include "Components/BrushComponent.h"
#include "Core/OBCollisionChannels.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogOBHelicopterExclusionVolume, Log, All);

AOBHelicopterExclusionVolume::AOBHelicopterExclusionVolume(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ConfigureLogicalVolumeCollision();
}

void AOBHelicopterExclusionVolume::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	ConfigureLogicalVolumeCollision();
}

void AOBHelicopterExclusionVolume::ConfigureLogicalVolumeCollision()
{
	UBrushComponent* VolumeBrushComponent = GetBrushComponent();
	if (!VolumeBrushComponent)
	{
		return;
	}

	VolumeBrushComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	VolumeBrushComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	VolumeBrushComponent->SetGenerateOverlapEvents(false);
	VolumeBrushComponent->SetCanEverAffectNavigation(false);

	ensureAlwaysMsgf(
		VolumeBrushComponent->GetCollisionResponseToChannel(OB_TraceChannel_Weapon) == ECR_Ignore
			&& VolumeBrushComponent->GetCollisionResponseToChannel(OB_TraceChannel_CameraProbe) == ECR_Ignore,
		TEXT("Helicopter exclusion volume %s must ignore Weapon and CameraProbe traces."),
		*GetName());

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		UE_LOG(LogOBHelicopterExclusionVolume, Log,
			TEXT("[InsertionExclusion] Logical collision enforced Volume=%s Collision=%s Weapon=%d CameraProbe=%d"),
			*GetName(), *UEnum::GetValueAsString(VolumeBrushComponent->GetCollisionEnabled()),
			static_cast<int32>(VolumeBrushComponent->GetCollisionResponseToChannel(OB_TraceChannel_Weapon)),
			static_cast<int32>(VolumeBrushComponent->GetCollisionResponseToChannel(OB_TraceChannel_CameraProbe)));
	}
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOBHelicopterExclusionVolumeCollisionContractTest,
	"OutBreak.Expedition.LogicalVolumes.ExclusionCollisionContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOBHelicopterExclusionVolumeCollisionContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const AOBHelicopterExclusionVolume* VolumeCDO = GetDefault<AOBHelicopterExclusionVolume>();
	const UBrushComponent* VolumeBrushComponent = VolumeCDO ? VolumeCDO->GetBrushComponent() : nullptr;
	if (!TestNotNull(TEXT("Exclusion volume CDO has a brush component"), VolumeBrushComponent))
	{
		return false;
	}

	TestEqual(TEXT("Exclusion volume keeps a query shape"),
		VolumeBrushComponent->GetCollisionEnabled(), ECollisionEnabled::QueryOnly);
	TestEqual(TEXT("Exclusion volume ignores Weapon traces"),
		VolumeBrushComponent->GetCollisionResponseToChannel(OB_TraceChannel_Weapon), ECR_Ignore);
	TestEqual(TEXT("Exclusion volume ignores CameraProbe traces"),
		VolumeBrushComponent->GetCollisionResponseToChannel(OB_TraceChannel_CameraProbe), ECR_Ignore);
	TestFalse(TEXT("Exclusion volume does not generate overlaps"),
		VolumeBrushComponent->GetGenerateOverlapEvents());
	return true;
}
#endif
