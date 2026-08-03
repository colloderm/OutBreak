#include "InteriorPCGTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "InteriorPCGPreset.h"
#include "InteriorPCGItemComponent.h"
#include "InteriorPCGVolume.h"
#include "InteriorPCGWallVolume.h"
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
	UInteriorPCGPreset* SecondPreset = NewObject<UInteriorPCGPreset>(GetTransientPackage());
	for (const FInteriorPCGPresetItem& Item : Preset->Items)
	{
		if (Item.bEnabled)
		{
			FInteriorPCGPresetItem& SecondItem = SecondPreset->Items.Add_GetRef(Item);
			SecondItem.NormalizedPosition = FVector2D(0.85, 0.85);
			break;
		}
	}
	TestEqual(TEXT("A second explicit preset contains one enabled placement"), SecondPreset->Items.Num(), 1);
	Volume->SelectedPresets = { Preset, SecondPreset };

	const int32 FirstBatchCount = Volume->GenerateFromSelectedPresets();
	TestEqual(TEXT("Preset batch generation combines every enabled placement"), FirstBatchCount, 4);
	TArray<AActor*> FirstBatchActors;
	Volume->GetRegisteredActors(FirstBatchActors);
	TMap<FGuid, FTransform> FirstBatchTransforms;
	for (AActor* Actor : FirstBatchActors)
	{
		if (const UInteriorPCGItemComponent* ItemComponent = Actor->FindComponentByClass<UInteriorPCGItemComponent>())
		{
			FirstBatchTransforms.Add(ItemComponent->StableId, Actor->GetActorTransform());
		}
	}
	TestEqual(TEXT("Duplicate preset item IDs are deterministically remapped to unique IDs"), FirstBatchTransforms.Num(), FirstBatchCount);

	const int32 SecondBatchCount = Volume->GenerateFromSelectedPresets();
	TestEqual(TEXT("Second preset batch generation reproduces the same count"), SecondBatchCount, FirstBatchCount);
	TArray<AActor*> SecondBatchActors;
	Volume->GetRegisteredActors(SecondBatchActors);
	for (AActor* Actor : SecondBatchActors)
	{
		const UInteriorPCGItemComponent* ItemComponent = Actor->FindComponentByClass<UInteriorPCGItemComponent>();
		const FTransform* FirstTransform = ItemComponent ? FirstBatchTransforms.Find(ItemComponent->StableId) : nullptr;
		TestNotNull(TEXT("Preset batch stable IDs are deterministic"), FirstTransform);
		if (FirstTransform)
		{
			TestTrue(TEXT("Preset batch transforms are deterministic"), FirstTransform->Equals(Actor->GetActorTransform(), 0.01f));
		}
	}
	Volume->ClearGeneratedActors();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInteriorPCGWallWorkflowTest,
	"OutBreak.InteriorPCG.WallWorkflow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInteriorPCGWallWorkflowTest::RunTest(const FString& Parameters)
{
	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	TestNotNull(TEXT("Wall workflow cube mesh loads"), CubeMesh);
	if (!CubeMesh)
	{
		return false;
	}

	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	TestNotNull(TEXT("Wall workflow temporary editor world is available"), World);
	if (!World)
	{
		return false;
	}

	auto SpawnBlockingCube = [World, CubeMesh](const FVector& Location, const FVector& Scale)
	{
		AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(Location));
		if (Actor)
		{
			Actor->GetStaticMeshComponent()->SetStaticMesh(CubeMesh);
			Actor->GetStaticMeshComponent()->SetWorldScale3D(Scale);
			Actor->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Actor->GetStaticMeshComponent()->SetCollisionResponseToAllChannels(ECR_Block);
		}
		return Actor;
	};

	TestNotNull(TEXT("Wall workflow floor spawns"), SpawnBlockingCube(FVector(0.0, 0.0, -5.0), FVector(10.0, 10.0, 0.1)));
	TestNotNull(TEXT("Negative X boundary wall spawns"), SpawnBlockingCube(FVector(-400.0, 0.0, 150.0), FVector(0.2, 8.0, 3.0)));
	TestNotNull(TEXT("Positive X boundary wall spawns"), SpawnBlockingCube(FVector(400.0, 0.0, 150.0), FVector(0.2, 8.0, 3.0)));
	TestNotNull(TEXT("Negative Y boundary wall spawns"), SpawnBlockingCube(FVector(0.0, -400.0, 150.0), FVector(8.0, 0.2, 3.0)));
	TestNotNull(TEXT("Positive Y boundary wall spawns"), SpawnBlockingCube(FVector(0.0, 400.0, 150.0), FVector(8.0, 0.2, 3.0)));

	AInteriorPCGWallVolume* Volume = World->SpawnActor<AInteriorPCGWallVolume>(AInteriorPCGWallVolume::StaticClass(), FTransform(FVector(0.0, 0.0, 200.0)));
	TestNotNull(TEXT("Interior PCG Wall Volume spawns"), Volume);
	if (!Volume)
	{
		return false;
	}

	UCubeBuilder* VolumeBuilder = NewObject<UCubeBuilder>();
	VolumeBuilder->X = 800.0f;
	VolumeBuilder->Y = 800.0f;
	VolumeBuilder->Z = 400.0f;
	UActorFactory::CreateBrushForVolumeActor(Volume, VolumeBuilder);

	Volume->WallSeed = 98765;
	Volume->PartitionWallCount = 1;
	Volume->WallDirectionMode = EInteriorPCGWallDirectionMode::VolumeLocalX;
	Volume->WallModuleLength = 100.0f;
	Volume->WallScanHeight = 120.0f;
	Volume->WallEndClearance = 5.0f;
	Volume->PartitionMargin = 100.0f;
	Volume->MaxWallPlacementAttempts = 20;
	Volume->DoorChancePerPartition = 1.0f;
	Volume->DoorEndPaddingModules = 1;
	Volume->bCheckWorldCollision = true;
	Volume->bCheckPresetCollision = false;

	FInteriorPCGWallClassEntry& WallEntry = Volume->WallClasses.AddDefaulted_GetRef();
	WallEntry.Label = TEXT("ExplicitTestWallClass");
	WallEntry.EntryId = FGuid(10, 20, 30, 40);
	WallEntry.ActorClass = AStaticMeshActor::StaticClass();
	WallEntry.SelectionWeight = 1.0f;
	WallEntry.CollisionHalfExtent = FVector(49.0, 10.0, 150.0);

	FInteriorPCGWallClassEntry& DoorEntry = Volume->DoorWallClasses.AddDefaulted_GetRef();
	DoorEntry.Label = TEXT("ExplicitTestDoorWallClass");
	DoorEntry.EntryId = FGuid(50, 60, 70, 80);
	DoorEntry.ActorClass = AStaticMeshActor::StaticClass();
	DoorEntry.SelectionWeight = 1.0f;
	DoorEntry.CollisionHalfExtent = FVector(49.0, 10.0, 150.0);

	const int32 FirstWallCount = Volume->GenerateInteriorWalls();
	TestTrue(TEXT("Boundary scans create at least one editable wall module"), FirstWallCount > 0);

	TArray<AActor*> FirstActors;
	Volume->GetRegisteredActors(FirstActors);
	TMap<FGuid, FTransform> FirstWallTransforms;
	int32 FirstDoorCount = 0;
	for (AActor* Actor : FirstActors)
	{
		if (const UInteriorPCGItemComponent* ItemComponent = Actor->FindComponentByClass<UInteriorPCGItemComponent>())
		{
			if (ItemComponent->Role == EInteriorPCGItemRole::InteriorWall || ItemComponent->Role == EInteriorPCGItemRole::DoorWall)
			{
				FirstWallTransforms.Add(ItemComponent->StableId, Actor->GetActorTransform());
				FirstDoorCount += ItemComponent->Role == EInteriorPCGItemRole::DoorWall ? 1 : 0;
			}
		}
	}
	TestEqual(TEXT("Exactly one door-wall module is selected for the partition"), FirstDoorCount, 1);
	TestEqual(TEXT("Every reported wall module is registered"), FirstWallTransforms.Num(), FirstWallCount);

	const int32 SecondWallCount = Volume->GenerateInteriorWalls();
	TestEqual(TEXT("The same Wall Seed reproduces the module count"), SecondWallCount, FirstWallCount);
	TArray<AActor*> SecondActors;
	Volume->GetRegisteredActors(SecondActors);
	for (AActor* Actor : SecondActors)
	{
		const UInteriorPCGItemComponent* ItemComponent = Actor->FindComponentByClass<UInteriorPCGItemComponent>();
		if (!ItemComponent)
		{
			continue;
		}
		const FTransform* FirstTransform = FirstWallTransforms.Find(ItemComponent->StableId);
		TestNotNull(TEXT("The same Wall Seed reproduces every stable wall ID"), FirstTransform);
		if (FirstTransform)
		{
			TestTrue(TEXT("The same Wall Seed reproduces every wall transform"), FirstTransform->Equals(Actor->GetActorTransform(), 0.01f));
		}
	}

	AStaticMeshActor* UserProp = SpawnBlockingCube(FVector(0.0, 300.0, 50.0), FVector(0.5, 0.5, 0.5));
	TestTrue(TEXT("A user prop can be registered on the child volume"), Volume->RegisterActor(UserProp));
	TestEqual(TEXT("Clearing walls removes wall roles only"), Volume->ClearGeneratedWalls(), SecondWallCount);
	TArray<AActor*> ActorsAfterWallClear;
	Volume->GetRegisteredActors(ActorsAfterWallClear);
	TestEqual(TEXT("The registered furniture/prop survives wall-only clear"), ActorsAfterWallClear.Num(), 1);

	TestEqual(TEXT("Walls can be regenerated while registered furniture remains"), Volume->GenerateInteriorWalls(), FirstWallCount);
	UInteriorPCGPreset* Preset = NewObject<UInteriorPCGPreset>(GetTransientPackage());
	const int32 CapturedCount = Volume->CaptureCurrentPlacement(Preset);
	TestEqual(TEXT("Preset captures the furniture and all wall modules"), CapturedCount, FirstWallCount + 1);
	int32 PresetDoorCount = 0;
	for (const FInteriorPCGPresetItem& Item : Preset->Items)
	{
		PresetDoorCount += Item.Role == EInteriorPCGItemRole::DoorWall ? 1 : 0;
	}
	TestEqual(TEXT("Preset preserves the door-wall role"), PresetDoorCount, 1);

	Volume->ClearGeneratedActors();
	TestEqual(TEXT("Preset rebuild restores furniture and wall modules"), Volume->GenerateFromPreset(Preset), CapturedCount);
	int32 RebuiltDoorCount = 0;
	TArray<AActor*> RebuiltActors;
	Volume->GetRegisteredActors(RebuiltActors);
	for (AActor* Actor : RebuiltActors)
	{
		if (const UInteriorPCGItemComponent* ItemComponent = Actor->FindComponentByClass<UInteriorPCGItemComponent>())
		{
			RebuiltDoorCount += ItemComponent->Role == EInteriorPCGItemRole::DoorWall ? 1 : 0;
		}
	}
	TestEqual(TEXT("Preset rebuild restores the door-wall role on actor metadata"), RebuiltDoorCount, 1);
	Volume->ClearGeneratedActors();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInteriorPCGMultiFloorWorkflowTest,
	"OutBreak.InteriorPCG.ZZMultiFloorWorkflow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInteriorPCGMultiFloorWorkflowTest::RunTest(const FString& Parameters)
{
	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	TestNotNull(TEXT("Multi-floor workflow cube mesh loads"), CubeMesh);
	if (!CubeMesh)
	{
		return false;
	}

	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	TestNotNull(TEXT("Multi-floor temporary editor world is available"), World);
	if (!World)
	{
		return false;
	}

	auto SpawnBlockingCube = [World, CubeMesh](const FVector& Location, const FVector& Scale)
	{
		AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(Location));
		if (Actor)
		{
			Actor->GetStaticMeshComponent()->SetStaticMesh(CubeMesh);
			Actor->GetStaticMeshComponent()->SetWorldScale3D(Scale);
			Actor->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Actor->GetStaticMeshComponent()->SetCollisionResponseToAllChannels(ECR_Block);
		}
		return Actor;
	};

	TestNotNull(TEXT("Ground floor spawns"), SpawnBlockingCube(FVector(0.0, 0.0, -5.0), FVector(10.0, 10.0, 0.1)));
	TestNotNull(TEXT("Second floor spawns"), SpawnBlockingCube(FVector(0.0, 0.0, 295.0), FVector(10.0, 10.0, 0.1)));
	TestNotNull(TEXT("Multi-floor negative X boundary wall spawns"), SpawnBlockingCube(FVector(-400.0, 0.0, 300.0), FVector(0.2, 8.0, 6.0)));
	TestNotNull(TEXT("Multi-floor positive X boundary wall spawns"), SpawnBlockingCube(FVector(400.0, 0.0, 300.0), FVector(0.2, 8.0, 6.0)));
	TestNotNull(TEXT("Multi-floor negative Y boundary wall spawns"), SpawnBlockingCube(FVector(0.0, -400.0, 300.0), FVector(8.0, 0.2, 6.0)));
	TestNotNull(TEXT("Multi-floor positive Y boundary wall spawns"), SpawnBlockingCube(FVector(0.0, 400.0, 300.0), FVector(8.0, 0.2, 6.0)));

	AInteriorPCGWallVolume* Volume = World->SpawnActor<AInteriorPCGWallVolume>(AInteriorPCGWallVolume::StaticClass(), FTransform(FVector(0.0, 0.0, 300.0)));
	TestNotNull(TEXT("Multi-floor wall volume spawns"), Volume);
	if (!Volume)
	{
		return false;
	}

	UCubeBuilder* VolumeBuilder = NewObject<UCubeBuilder>();
	VolumeBuilder->X = 800.0f;
	VolumeBuilder->Y = 800.0f;
	VolumeBuilder->Z = 700.0f;
	UActorFactory::CreateBrushForVolumeActor(Volume, VolumeBuilder);

	Volume->bGenerateOnAllDetectedFloors = true;
	Volume->FloorDetectionSamplesPerAxis = 3;
	Volume->FloorLayerHeightTolerance = 20.0f;
	Volume->MinimumFloorSampleCoverage = 0.8f;
	Volume->MaximumDetectedFloorCount = 4;
	TestEqual(TEXT("Automatic floor scan detects two layers"), Volume->ScanFloorLayers(), 2);
	if (Volume->LastDetectedFloorWorldHeights.Num() == 2)
	{
		TestTrue(TEXT("Ground floor height is detected"), FMath::IsNearlyEqual(Volume->LastDetectedFloorWorldHeights[0], 0.0f, 1.0f));
		TestTrue(TEXT("Second floor height is detected"), FMath::IsNearlyEqual(Volume->LastDetectedFloorWorldHeights[1], 300.0f, 1.0f));
	}

	Volume->Seed = 24680;
	Volume->MaxPlacementAttemptsPerItem = 100;
	Volume->bCheckWorldCollision = true;
	Volume->bCheckPresetCollision = false;
	FInteriorPCGAssetEntry& FurnitureEntry = Volume->AssetEntries.AddDefaulted_GetRef();
	FurnitureEntry.Label = TEXT("MultiFloorFurniture");
	FurnitureEntry.EntryId = FGuid(101, 102, 103, 104);
	FurnitureEntry.AssetKind = EInteriorPCGAssetKind::StaticMesh;
	FurnitureEntry.StaticMesh = CubeMesh;
	FurnitureEntry.QuantityMode = EInteriorPCGQuantityMode::FixedCount;
	FurnitureEntry.Count = 1;
	FurnitureEntry.CollisionHalfExtent = FVector(45.0, 45.0, 45.0);
	FurnitureEntry.PositionOffset = FVector(0.0, 0.0, 50.0);

	TestEqual(TEXT("One furniture actor is generated per detected floor"), Volume->GenerateRandomInterior(), 2);
	TArray<AActor*> FirstFurnitureActors;
	Volume->GetRegisteredActors(FirstFurnitureActors);
	TMap<FGuid, FTransform> FirstFurnitureTransforms;
	TMap<int32, FVector> FurnitureLocationsByFloor;
	for (AActor* Actor : FirstFurnitureActors)
	{
		if (const UInteriorPCGItemComponent* ItemComponent = Actor->FindComponentByClass<UInteriorPCGItemComponent>())
		{
			FirstFurnitureTransforms.Add(ItemComponent->StableId, Actor->GetActorTransform());
			FurnitureLocationsByFloor.Add(ItemComponent->FloorIndex, Actor->GetActorLocation());
		}
	}
	TestEqual(TEXT("Furniture metadata records both floor indices"), FurnitureLocationsByFloor.Num(), 2);
	if (const FVector* GroundLocation = FurnitureLocationsByFloor.Find(0))
	{
		if (const FVector* UpperLocation = FurnitureLocationsByFloor.Find(1))
		{
			TestFalse(TEXT("The same base Seed produces different XY randomness on each floor"), FVector2D(*GroundLocation).Equals(FVector2D(*UpperLocation), 0.01f));
		}
	}

	TestEqual(TEXT("Multi-floor furniture generation reproduces its count"), Volume->GenerateRandomInterior(), 2);
	TArray<AActor*> SecondFurnitureActors;
	Volume->GetRegisteredActors(SecondFurnitureActors);
	for (AActor* Actor : SecondFurnitureActors)
	{
		const UInteriorPCGItemComponent* ItemComponent = Actor->FindComponentByClass<UInteriorPCGItemComponent>();
		const FTransform* FirstTransform = ItemComponent ? FirstFurnitureTransforms.Find(ItemComponent->StableId) : nullptr;
		TestNotNull(TEXT("Multi-floor furniture stable ID is reproducible"), FirstTransform);
		if (FirstTransform)
		{
			TestTrue(TEXT("Multi-floor furniture transform is reproducible"), FirstTransform->Equals(Actor->GetActorTransform(), 0.01f));
		}
	}

	UInteriorPCGPreset* MultiFloorPreset = NewObject<UInteriorPCGPreset>(GetTransientPackage());
	TestEqual(TEXT("Multi-floor preset captures one item for each floor"), Volume->CaptureCurrentPlacement(MultiFloorPreset), 2);
	TSet<int32> CapturedFloorIndices;
	for (const FInteriorPCGPresetItem& Item : MultiFloorPreset->Items)
	{
		CapturedFloorIndices.Add(Item.FloorIndex);
	}
	TestTrue(TEXT("Multi-floor preset stores floor zero"), CapturedFloorIndices.Contains(0));
	TestTrue(TEXT("Multi-floor preset stores floor one"), CapturedFloorIndices.Contains(1));
	TestEqual(TEXT("Multi-floor preset restores items only on their stored floors"), Volume->GenerateFromPreset(MultiFloorPreset), 2);

	UInteriorPCGPreset* LegacySingleFloorPreset = NewObject<UInteriorPCGPreset>(GetTransientPackage());
	if (!MultiFloorPreset->Items.IsEmpty())
	{
		FInteriorPCGPresetItem& LegacyItem = LegacySingleFloorPreset->Items.Add_GetRef(MultiFloorPreset->Items[0]);
		LegacyItem.FloorIndex = INDEX_NONE;
	}
	TestEqual(TEXT("A legacy single-floor preset repeats once per detected floor"), Volume->GenerateFromPreset(LegacySingleFloorPreset), 2);
	Volume->ClearGeneratedActors();

	Volume->WallSeed = 13579;
	Volume->PartitionWallCount = 1;
	Volume->WallDirectionMode = EInteriorPCGWallDirectionMode::VolumeLocalX;
	Volume->WallModuleLength = 100.0f;
	Volume->WallScanHeight = 80.0f;
	Volume->WallEndClearance = 5.0f;
	Volume->DoorChancePerPartition = 1.0f;
	Volume->DoorEndPaddingModules = 1;
	FInteriorPCGWallClassEntry& WallEntry = Volume->WallClasses.AddDefaulted_GetRef();
	WallEntry.Label = TEXT("MultiFloorWall");
	WallEntry.EntryId = FGuid(201, 202, 203, 204);
	WallEntry.ActorClass = AStaticMeshActor::StaticClass();
	WallEntry.CollisionHalfExtent = FVector(49.0, 10.0, 100.0);
	FInteriorPCGWallClassEntry& DoorEntry = Volume->DoorWallClasses.AddDefaulted_GetRef();
	DoorEntry.Label = TEXT("MultiFloorDoorWall");
	DoorEntry.EntryId = FGuid(205, 206, 207, 208);
	DoorEntry.ActorClass = AStaticMeshActor::StaticClass();
	DoorEntry.CollisionHalfExtent = FVector(49.0, 10.0, 100.0);

	const int32 MultiFloorWallCount = Volume->GenerateInteriorWalls();
	TestTrue(TEXT("Interior walls are generated on both detected floors"), MultiFloorWallCount > 0 && MultiFloorWallCount % 2 == 0);
	TMap<int32, int32> WallCountsByFloor;
	TMap<int32, int32> DoorCountsByFloor;
	TArray<AActor*> WallActors;
	Volume->GetRegisteredActors(WallActors);
	for (AActor* Actor : WallActors)
	{
		if (const UInteriorPCGItemComponent* ItemComponent = Actor->FindComponentByClass<UInteriorPCGItemComponent>())
		{
			++WallCountsByFloor.FindOrAdd(ItemComponent->FloorIndex);
			if (ItemComponent->Role == EInteriorPCGItemRole::DoorWall)
			{
				++DoorCountsByFloor.FindOrAdd(ItemComponent->FloorIndex);
			}
		}
	}
	TestEqual(TEXT("Wall generation records two floor groups"), WallCountsByFloor.Num(), 2);
	TestEqual(TEXT("Ground floor receives one door-wall module"), DoorCountsByFloor.FindRef(0), 1);
	TestEqual(TEXT("Second floor receives one door-wall module"), DoorCountsByFloor.FindRef(1), 1);

	UInteriorPCGPreset* MultiFloorWallPreset = NewObject<UInteriorPCGPreset>(GetTransientPackage());
	TestEqual(TEXT("Multi-floor wall preset captures every generated module"), Volume->CaptureCurrentPlacement(MultiFloorWallPreset), MultiFloorWallCount);
	TestEqual(TEXT("Multi-floor wall preset restores every module on its floor"), Volume->GenerateFromPreset(MultiFloorWallPreset), MultiFloorWallCount);
	Volume->ClearGeneratedActors();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInteriorPCGConnectedCirculationWorkflowTest,
	"OutBreak.InteriorPCG.ZZZConnectedCirculationWorkflow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInteriorPCGConnectedCirculationWorkflowTest::RunTest(const FString& Parameters)
{
	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	TestNotNull(TEXT("Connected circulation cube mesh loads"), CubeMesh);
	if (!CubeMesh)
	{
		return false;
	}

	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	TestNotNull(TEXT("Connected circulation temporary editor world is available"), World);
	if (!World)
	{
		return false;
	}

	auto SpawnBlockingCube = [World, CubeMesh](const FVector& Location, const FVector& Scale)
	{
		AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(Location));
		if (Actor)
		{
			Actor->GetStaticMeshComponent()->SetStaticMesh(CubeMesh);
			Actor->GetStaticMeshComponent()->SetWorldScale3D(Scale);
			Actor->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Actor->GetStaticMeshComponent()->SetCollisionResponseToAllChannels(ECR_Block);
		}
		return Actor;
	};

	TestNotNull(TEXT("Connected circulation floor zero spawns"), SpawnBlockingCube(FVector(0.0, 0.0, -5.0), FVector(10.0, 10.0, 0.1)));
	TestNotNull(TEXT("Connected circulation floor one spawns"), SpawnBlockingCube(FVector(0.0, 0.0, 295.0), FVector(10.0, 10.0, 0.1)));
	TestNotNull(TEXT("Connected circulation floor two spawns"), SpawnBlockingCube(FVector(0.0, 0.0, 595.0), FVector(10.0, 10.0, 0.1)));
	TestNotNull(TEXT("Connected circulation negative X wall spawns"), SpawnBlockingCube(FVector(-400.0, 0.0, 450.0), FVector(0.2, 8.0, 9.0)));
	TestNotNull(TEXT("Connected circulation positive X wall spawns"), SpawnBlockingCube(FVector(400.0, 0.0, 450.0), FVector(0.2, 8.0, 9.0)));
	TestNotNull(TEXT("Connected circulation negative Y wall spawns"), SpawnBlockingCube(FVector(0.0, -400.0, 450.0), FVector(8.0, 0.2, 9.0)));
	TestNotNull(TEXT("Connected circulation positive Y wall spawns"), SpawnBlockingCube(FVector(0.0, 400.0, 450.0), FVector(8.0, 0.2, 9.0)));

	AInteriorPCGWallVolume* Volume = World->SpawnActor<AInteriorPCGWallVolume>(AInteriorPCGWallVolume::StaticClass(), FTransform(FVector(0.0, 0.0, 450.0)));
	TestNotNull(TEXT("Connected circulation wall volume spawns"), Volume);
	if (!Volume)
	{
		return false;
	}

	UCubeBuilder* VolumeBuilder = NewObject<UCubeBuilder>();
	VolumeBuilder->X = 800.0f;
	VolumeBuilder->Y = 800.0f;
	VolumeBuilder->Z = 1000.0f;
	UActorFactory::CreateBrushForVolumeActor(Volume, VolumeBuilder);

	Volume->bGenerateOnAllDetectedFloors = true;
	Volume->FloorDetectionSamplesPerAxis = 3;
	Volume->FloorLayerHeightTolerance = 20.0f;
	Volume->MinimumFloorSampleCoverage = 0.8f;
	Volume->MaximumDetectedFloorCount = 4;
	Volume->bRequireConnectedDoorAndStairPaths = true;
	Volume->WallSeed = 112233;
	Volume->PartitionWallCount = 1;
	Volume->WallDirectionMode = EInteriorPCGWallDirectionMode::VolumeLocalX;
	Volume->WallModuleLength = 100.0f;
	Volume->WallScanHeight = 80.0f;
	Volume->WallEndClearance = 5.0f;
	Volume->PartitionMargin = 100.0f;
	Volume->MaxWallPlacementAttempts = 200;
	Volume->MaxStairPlacementAttempts = 100;
	Volume->DoorEndPaddingModules = 0;
	Volume->WalkwayHalfWidth = 20.0f;
	Volume->WalkwayHalfHeight = 80.0f;
	Volume->WalkwaySampleSpacing = 40.0f;
	Volume->bCheckWorldCollision = true;
	Volume->bCheckPresetCollision = false;

	FInteriorPCGWallClassEntry& WallEntry = Volume->WallClasses.AddDefaulted_GetRef();
	WallEntry.Label = TEXT("ConnectedWallActorClass");
	WallEntry.EntryId = FGuid(301, 302, 303, 304);
	WallEntry.AssetKind = EInteriorPCGAssetKind::ActorClass;
	WallEntry.ActorClass = AStaticMeshActor::StaticClass();
	WallEntry.CollisionHalfExtent = FVector(49.0, 10.0, 100.0);

	FInteriorPCGWallClassEntry& DoorEntry = Volume->DoorWallClasses.AddDefaulted_GetRef();
	DoorEntry.Label = TEXT("ConnectedDoorActorClass");
	DoorEntry.EntryId = FGuid(305, 306, 307, 308);
	DoorEntry.AssetKind = EInteriorPCGAssetKind::ActorClass;
	DoorEntry.ActorClass = AStaticMeshActor::StaticClass();
	DoorEntry.CollisionHalfExtent = FVector(49.0, 10.0, 100.0);
	DoorEntry.DoorAccessPointOffset = FVector::ZeroVector;

	FInteriorPCGWallClassEntry& StairEntry = Volume->StairClasses.AddDefaulted_GetRef();
	StairEntry.Label = TEXT("ConnectedStairStaticMesh");
	StairEntry.EntryId = FGuid(309, 310, 311, 312);
	StairEntry.AssetKind = EInteriorPCGAssetKind::StaticMesh;
	StairEntry.StaticMesh = CubeMesh;
	StairEntry.CollisionHalfExtent = FVector(40.0, 40.0, 80.0);
	StairEntry.LowerAccessPointOffset = FVector(120.0, 0.0, 0.0);
	StairEntry.UpperAccessPointOffset = FVector(-120.0, 0.0, 0.0);

	TestEqual(TEXT("Connected circulation scan detects three floors"), Volume->ScanFloorLayers(), 3);
	const int32 FirstGeneratedCount = Volume->GenerateInteriorWalls();
	TestEqual(TEXT("Three connected partitions and two stairs are generated"), FirstGeneratedCount, 23);
	TestTrue(TEXT("Connectivity generation records reserved walkway samples"), !Volume->LastConnectivityPathPoints.IsEmpty());

	TArray<AActor*> FirstActors;
	Volume->GetRegisteredActors(FirstActors);
	TMap<FGuid, FTransform> FirstTransforms;
	TMap<int32, int32> DoorCountsByFloor;
	TMap<int32, int32> StairCountsByFloor;
	int32 StaticMeshStairCount = 0;
	for (AActor* Actor : FirstActors)
	{
		if (const UInteriorPCGItemComponent* ItemComponent = Actor->FindComponentByClass<UInteriorPCGItemComponent>())
		{
			FirstTransforms.Add(ItemComponent->StableId, Actor->GetActorTransform());
			if (ItemComponent->Role == EInteriorPCGItemRole::DoorWall)
			{
				++DoorCountsByFloor.FindOrAdd(ItemComponent->FloorIndex);
			}
			else if (ItemComponent->Role == EInteriorPCGItemRole::Stair)
			{
				++StairCountsByFloor.FindOrAdd(ItemComponent->FloorIndex);
				StaticMeshStairCount += ItemComponent->AssetKind == EInteriorPCGAssetKind::StaticMesh ? 1 : 0;
			}
		}
	}
	TestEqual(TEXT("Every floor receives exactly one connected door module"), DoorCountsByFloor.Num(), 3);
	TestEqual(TEXT("Floor zero receives one door"), DoorCountsByFloor.FindRef(0), 1);
	TestEqual(TEXT("Floor one receives one door"), DoorCountsByFloor.FindRef(1), 1);
	TestEqual(TEXT("Floor two receives one door"), DoorCountsByFloor.FindRef(2), 1);
	TestEqual(TEXT("Two floor transitions receive stair actors"), StairCountsByFloor.Num(), 2);
	TestEqual(TEXT("Both stairs use the explicitly registered Static Mesh entry"), StaticMeshStairCount, 2);

	const TArray<FVector> FirstPathPoints = Volume->LastConnectivityPathPoints;
	TestEqual(TEXT("Connected circulation regeneration reproduces the actor count"), Volume->GenerateInteriorWalls(), FirstGeneratedCount);
	TestEqual(TEXT("Connected circulation regeneration reproduces the path sample count"), Volume->LastConnectivityPathPoints.Num(), FirstPathPoints.Num());
	TArray<AActor*> SecondActors;
	Volume->GetRegisteredActors(SecondActors);
	for (AActor* Actor : SecondActors)
	{
		const UInteriorPCGItemComponent* ItemComponent = Actor->FindComponentByClass<UInteriorPCGItemComponent>();
		const FTransform* FirstTransform = ItemComponent ? FirstTransforms.Find(ItemComponent->StableId) : nullptr;
		TestNotNull(TEXT("Connected circulation stable IDs are deterministic"), FirstTransform);
		if (FirstTransform)
		{
			TestTrue(TEXT("Connected circulation transforms are deterministic"), FirstTransform->Equals(Actor->GetActorTransform(), 0.01f));
		}
	}

	UInteriorPCGPreset* ConnectedPreset = NewObject<UInteriorPCGPreset>(GetTransientPackage());
	TestEqual(TEXT("Connected preset captures walls, doors, and stairs"), Volume->CaptureCurrentPlacement(ConnectedPreset), FirstGeneratedCount);
	int32 PresetStairCount = 0;
	for (const FInteriorPCGPresetItem& Item : ConnectedPreset->Items)
	{
		PresetStairCount += Item.Role == EInteriorPCGItemRole::Stair ? 1 : 0;
	}
	TestEqual(TEXT("Connected preset preserves both stair roles"), PresetStairCount, 2);
	TestEqual(TEXT("Connected preset restores every actor on its floor"), Volume->GenerateFromPreset(ConnectedPreset), FirstGeneratedCount);
	Volume->ClearGeneratedActors();
	return true;
}

#endif
