#include "InteriorPCGTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "InteriorPCGPreset.h"
#include "InteriorPCGItemComponent.h"
#include "InteriorPCGVolume.h"
#include "PCGInteriorGeneratorSettings.h"
#include "ActorFactories/ActorFactory.h"
#include "Builders/CubeBuilder.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "PCGCommon.h"
#include "PCGGraph.h"
#include "PCGNode.h"
#include "Tests/AutomationEditorCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInteriorPCGDeterminismTest,
	"OutBreak.InteriorPCG.DeterministicRandomStream",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInteriorPCGDeterminismTest::RunTest(const FString& Parameters)
{
	FRandomStream StreamA(8675309);
	FRandomStream StreamB(8675309);
	for (int32 Index = 0; Index < 64; ++Index)
	{
		TestEqual(TEXT("Random yaw is deterministic"), FInteriorPCGPlacementMath::ResolveYaw(EInteriorPCGRotationMode::RandomYaw, 90.0f, StreamA), FInteriorPCGPlacementMath::ResolveYaw(EInteriorPCGRotationMode::RandomYaw, 90.0f, StreamB));
		TestEqual(TEXT("Stable ID is deterministic"), FInteriorPCGPlacementMath::MakeStableGuid(StreamA), FInteriorPCGPlacementMath::MakeStableGuid(StreamB));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInteriorPCGNormalizedPositionTest,
	"OutBreak.InteriorPCG.NormalizedPositionRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInteriorPCGNormalizedPositionTest::RunTest(const FString& Parameters)
{
	const FBox Bounds(FVector(-400.0, -250.0, -10.0), FVector(600.0, 750.0, 300.0));
	const FVector Original(175.0, 325.0, 42.0);
	const FVector2D Normalized = FInteriorPCGPlacementMath::NormalizeLocalXY(Original, Bounds);
	const FVector Restored = FInteriorPCGPlacementMath::DenormalizeLocalXY(Normalized, Bounds, Original.Z);
	TestTrue(TEXT("Normalized XY round-trips"), Original.Equals(Restored, KINDA_SMALL_NUMBER));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInteriorPCGExampleAssetsTest,
	"OutBreak.InteriorPCG.GeneratedExampleAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInteriorPCGExampleAssetsTest::RunTest(const FString& Parameters)
{
	UPCGGraph* Graph = LoadObject<UPCGGraph>(nullptr, TEXT("/Game/InteriorPCG/PCG_InteriorGenerator.PCG_InteriorGenerator"));
	TestNotNull(TEXT("The generated PCG graph loads"), Graph);
	if (Graph)
	{
		const TArray<UPCGNode*> GeneratorNodes = Graph->FindNodesWithSettings(UPCGInteriorGeneratorSettings::StaticClass());
		TestEqual(TEXT("The generated graph has one Interior Generator node"), GeneratorNodes.Num(), 1);
		if (GeneratorNodes.Num() == 1)
		{
			TestTrue(TEXT("Graph input is connected to the Interior Generator"), GeneratorNodes[0]->IsInputPinConnected(PCGPinConstants::DefaultInputLabel));
			TestTrue(TEXT("Interior Generator is connected to graph output"), GeneratorNodes[0]->IsOutputPinConnected(PCGPinConstants::DefaultOutputLabel));
		}
	}

	UInteriorPCGPreset* Preset = LoadObject<UInteriorPCGPreset>(nullptr, TEXT("/Game/InteriorPCG/Presets/PCGPreset_EmptyExample.PCGPreset_EmptyExample"));
	TestNotNull(TEXT("The generated example PCG Preset loads"), Preset);
	if (Preset)
	{
		TestEqual(TEXT("The example preset intentionally starts empty"), Preset->Items.Num(), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInteriorPCGRoundTripWorkflowTest,
	"OutBreak.InteriorPCG.RoundTripWorkflow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInteriorPCGRoundTripWorkflowTest::RunTest(const FString& Parameters)
{
	AddExpectedError(
		TEXT("Unable to find RecastNavMesh instance while trying to create UCrowdManager instance"),
		EAutomationExpectedErrorFlags::Exact,
		1);

	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* CylinderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	TestNotNull(TEXT("Explicit test cube mesh loads"), CubeMesh);
	TestNotNull(TEXT("Explicit test cylinder mesh loads"), CylinderMesh);
	if (!CubeMesh || !CylinderMesh)
	{
		return false;
	}

	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	TestNotNull(TEXT("A temporary editor world is available"), World);
	if (!World)
	{
		return false;
	}

	AStaticMeshActor* Floor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(FVector(0.0, 0.0, -5.0)));
	TestNotNull(TEXT("Test floor spawns"), Floor);
	if (!Floor)
	{
		return false;
	}
	Floor->GetStaticMeshComponent()->SetStaticMesh(CubeMesh);
	Floor->GetStaticMeshComponent()->SetWorldScale3D(FVector(10.0, 10.0, 0.1));
	Floor->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Floor->GetStaticMeshComponent()->SetCollisionResponseToAllChannels(ECR_Block);

	AInteriorPCGVolume* Volume = World->SpawnActor<AInteriorPCGVolume>(AInteriorPCGVolume::StaticClass(), FTransform(FVector(0.0, 0.0, 200.0)));
	TestNotNull(TEXT("Interior PCG Volume spawns"), Volume);
	if (!Volume)
	{
		return false;
	}

	UCubeBuilder* VolumeBuilder = NewObject<UCubeBuilder>();
	VolumeBuilder->X = 800.0f;
	VolumeBuilder->Y = 800.0f;
	VolumeBuilder->Z = 400.0f;
	UActorFactory::CreateBrushForVolumeActor(Volume, VolumeBuilder);
	TestTrue(TEXT("The generated volume brush contains the floor interior"), Volume->EncompassesPoint(FVector(0.0, 0.0, 2.0)));

	Volume->Seed = 424242;
	Volume->MaxPlacementAttemptsPerItem = 100;
	Volume->WeightedSelectionCount = 0;
	Volume->bCheckWorldCollision = true;
	Volume->bCheckPresetCollision = false;
	FInteriorPCGAssetEntry& Entry = Volume->AssetEntries.AddDefaulted_GetRef();
	Entry.Label = TEXT("ExplicitCubeTestEntry");
	Entry.EntryId = FGuid(1, 2, 3, 4);
	Entry.AssetKind = EInteriorPCGAssetKind::StaticMesh;
	Entry.StaticMesh = CubeMesh;
	Entry.QuantityMode = EInteriorPCGQuantityMode::FixedCount;
	Entry.Count = 3;
	Entry.CollisionHalfExtent = FVector(45.0, 45.0, 45.0);
	Entry.PositionOffset = FVector(0.0, 0.0, 50.0);
	Entry.RotationMode = EInteriorPCGRotationMode::SteppedRandomYaw;
	Entry.YawStepDegrees = 90.0f;

	const int32 FirstGeneratedCount = Volume->GenerateRandomInterior();
	TestEqual(TEXT("Random generation creates the requested actor count"), FirstGeneratedCount, 3);

	TArray<AActor*> FirstActors;
	Volume->GetRegisteredActors(FirstActors);
	TMap<FGuid, FTransform> FirstTransforms;
	for (AActor* Actor : FirstActors)
	{
		if (const UInteriorPCGItemComponent* ItemComponent = Actor->FindComponentByClass<UInteriorPCGItemComponent>())
		{
			FirstTransforms.Add(ItemComponent->StableId, Actor->GetActorTransform());
		}
	}
	TestEqual(TEXT("Every random actor has a stable ID"), FirstTransforms.Num(), 3);

	const int32 SecondGeneratedCount = Volume->GenerateRandomInterior();
	TestEqual(TEXT("Second random generation creates the same count"), SecondGeneratedCount, 3);
	TArray<AActor*> SecondActors;
	Volume->GetRegisteredActors(SecondActors);
	for (AActor* Actor : SecondActors)
	{
		const UInteriorPCGItemComponent* ItemComponent = Actor->FindComponentByClass<UInteriorPCGItemComponent>();
		const FTransform* FirstTransform = ItemComponent ? FirstTransforms.Find(ItemComponent->StableId) : nullptr;
		TestNotNull(TEXT("The same seed reproduces each stable ID"), FirstTransform);
		if (FirstTransform)
		{
			TestTrue(TEXT("The same seed reproduces each transform"), FirstTransform->Equals(Actor->GetActorTransform(), 0.01f));
		}
	}

	if (SecondActors.Num() < 3)
	{
		return false;
	}

	AActor* EditedActor = SecondActors[0];
	UInteriorPCGItemComponent* EditedItemComponent = EditedActor->FindComponentByClass<UInteriorPCGItemComponent>();
	TestNotNull(TEXT("Edited actor has metadata"), EditedItemComponent);
	if (!EditedItemComponent)
	{
		return false;
	}
	const FGuid EditedStableId = EditedItemComponent->StableId;
	EditedActor->SetActorLocation(EditedActor->GetActorLocation() + FVector(0.0, 0.0, 25.0));
	EditedActor->SetActorRotation(FRotator(0.0, 37.0, 0.0));
	EditedActor->SetActorScale3D(FVector(1.2, 0.8, 1.1));
	CastChecked<AStaticMeshActor>(EditedActor)->GetStaticMeshComponent()->SetStaticMesh(CylinderMesh);
	const FTransform EditedTransform = EditedActor->GetActorTransform();

	UInteriorPCGItemComponent* ExcludedItemComponent = SecondActors[1]->FindComponentByClass<UInteriorPCGItemComponent>();
	TestNotNull(TEXT("Excluded actor has metadata"), ExcludedItemComponent);
	if (ExcludedItemComponent)
	{
		ExcludedItemComponent->bIncludedInPreset = false;
	}

	AStaticMeshActor* UserProp = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(FVector(-300.0, -300.0, 50.0)));
	UserProp->GetStaticMeshComponent()->SetStaticMesh(CubeMesh);
	UserProp->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	TestTrue(TEXT("A user-added prop can be registered"), Volume->RegisterActor(UserProp));

	UInteriorPCGPreset* Preset = NewObject<UInteriorPCGPreset>(GetTransientPackage());
	const int32 CapturedCount = Volume->CaptureCurrentPlacement(Preset);
	TestEqual(TEXT("Preset captures generated and user-added props"), CapturedCount, 4);
	TestEqual(TEXT("Clear removes all registered preview actors"), Volume->ClearGeneratedActors(), 4);

	const int32 PresetGeneratedCount = Volume->GenerateFromPreset(Preset);
	TestEqual(TEXT("Preset generation skips the explicitly excluded item"), PresetGeneratedCount, 3);
	TArray<AActor*> PresetActors;
	Volume->GetRegisteredActors(PresetActors);
	AActor* RebuiltEditedActor = nullptr;
	for (AActor* Actor : PresetActors)
	{
		if (const UInteriorPCGItemComponent* ItemComponent = Actor->FindComponentByClass<UInteriorPCGItemComponent>())
		{
			if (ItemComponent->StableId == EditedStableId)
			{
				RebuiltEditedActor = Actor;
				break;
			}
		}
	}
	TestNotNull(TEXT("The edited stable ID is restored from the preset"), RebuiltEditedActor);
	if (RebuiltEditedActor)
	{
		TestTrue(TEXT("Edited transform is restored from normalized XY and a new floor trace"), EditedTransform.Equals(RebuiltEditedActor->GetActorTransform(), 0.1f));
		const AStaticMeshActor* RebuiltStaticMeshActor = Cast<AStaticMeshActor>(RebuiltEditedActor);
		TestTrue(TEXT("Edited mesh asset is restored"), RebuiltStaticMeshActor && RebuiltStaticMeshActor->GetStaticMeshComponent()->GetStaticMesh() == CylinderMesh);
	}

	TestEqual(TEXT("Updating after preset generation preserves inactive records"), Volume->CaptureCurrentPlacement(Preset), 4);
	Volume->ClearGeneratedActors();
	return true;
}

#endif
