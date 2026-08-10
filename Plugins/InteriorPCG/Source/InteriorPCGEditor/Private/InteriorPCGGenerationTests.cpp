// Copyright OutBreak. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "InteriorPCGDataAssets.h"
#include "InteriorPCGGenerationLibrary.h"

namespace InteriorPCG::Tests
{
	UInteriorPCGGenerationProfile* MakeProfile()
	{
		UInteriorPCGGenerationProfile* Profile = NewObject<UInteriorPCGGenerationProfile>();
		Profile->BuildingRules = NewObject<UInteriorPCGBuildingRuleSet>(Profile);
		Profile->BuildingRules->CellSize = 400.0;
		Profile->BuildingRules->FloorHeight = 320.0;
		Profile->BuildingRules->CorridorWidthInCells = 2;
		Profile->BuildingRules->MinimumRoomLengthInCells = 2;
		Profile->BuildingRules->MaximumRoomLengthInCells = 4;
		Profile->BuildingRules->RepeatFloorVariation = EInteriorPCGFloorVariationMode::Identical;

		FInteriorPCGVerticalCoreDefinition& Stair = Profile->BuildingRules->VerticalCores.Emplace_GetRef();
		Stair.CoreID = TEXT("MainStair");
		Stair.ModuleType = EInteriorPCGModuleType::Stair;
		Stair.NormalizedPosition = FVector2D(0.5, 0.5);
		Stair.SizeInCells = FIntPoint(2, 2);

		FInteriorPCGWeightedRoomType& FirstOffice = Profile->BuildingRules->FirstFloorRoomTypes.Emplace_GetRef();
		FirstOffice.RoomType = EInteriorPCGRoomType::Office;
		FirstOffice.Weight = 1.0f;
		FInteriorPCGWeightedRoomType& RepeatOffice = Profile->BuildingRules->RepeatFloorRoomTypes.Emplace_GetRef();
		RepeatOffice.RoomType = EInteriorPCGRoomType::Office;
		RepeatOffice.Weight = 1.0f;
		return Profile;
	}

	FInteriorPCGGenerationOptions MakeOptions()
	{
		FInteriorPCGGenerationOptions Options;
		Options.Footprint = FVector2D(3200.0, 2400.0);
		Options.NumFloors = 3;
		Options.BuildingSeed = 8675309;
		Options.bGenerateStructure = true;
		Options.bGenerateInteriors = false;
		return Options;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteriorPCGDeterminismTest, "InteriorPCG.Generation.Determinism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInteriorPCGDeterminismTest::RunTest(const FString& Parameters)
{
	using namespace InteriorPCG::Tests;
	UInteriorPCGGenerationProfile* Profile = MakeProfile();
	const FInteriorPCGGenerationOptions Options = MakeOptions();
	FInteriorPCGGenerationResult First;
	FInteriorPCGGenerationResult Second;

	TestTrue(TEXT("First generation succeeds"), UInteriorPCGGenerationLibrary::Generate(Profile, Options, First));
	TestTrue(TEXT("Second generation succeeds"), UInteriorPCGGenerationLibrary::Generate(Profile, Options, Second));
	TestEqual(TEXT("Layout hash is deterministic"), First.LayoutHash, Second.LayoutHash);
	TestEqual(TEXT("Room count is deterministic"), First.Rooms.Num(), Second.Rooms.Num());
	TestEqual(TEXT("Placement count is deterministic"), First.Placements.Num(), Second.Placements.Num());
	TestTrue(TEXT("Rooms were generated"), !First.Rooms.IsEmpty());
	TestTrue(TEXT("Structure signals were generated without an asset set"), !First.Placements.IsEmpty());

	for (const FInteriorPCGRoom& Room : First.Rooms)
	{
		TestTrue(FString::Printf(TEXT("Room %d connects to the corridor"), Room.RoomID), Room.bConnectedToCorridor);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteriorPCGVerticalCoreTest, "InteriorPCG.Generation.VerticalCoreAndRepeatFloor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInteriorPCGVerticalCoreTest::RunTest(const FString& Parameters)
{
	using namespace InteriorPCG::Tests;
	UInteriorPCGGenerationProfile* Profile = MakeProfile();
	FInteriorPCGGenerationResult Result;
	TestTrue(TEXT("Generation succeeds"), UInteriorPCGGenerationLibrary::Generate(Profile, MakeOptions(), Result));

	TArray<const FInteriorPCGPlacement*> Stairs;
	for (const FInteriorPCGPlacement& Placement : Result.Placements)
	{
		if (Placement.ModuleType == EInteriorPCGModuleType::Stair) Stairs.Add(&Placement);
	}
	TestEqual(TEXT("A stair flight connects each adjacent floor pair"), Stairs.Num(), 2);
	if (Stairs.Num() == 2)
	{
		TestTrue(TEXT("Stair X aligns across floors"), FMath::IsNearlyEqual(Stairs[0]->Transform.GetLocation().X, Stairs[1]->Transform.GetLocation().X));
		TestTrue(TEXT("Stair Y aligns across floors"), FMath::IsNearlyEqual(Stairs[0]->Transform.GetLocation().Y, Stairs[1]->Transform.GetLocation().Y));
	}

	TArray<const FInteriorPCGRoom*> FloorOne;
	TArray<const FInteriorPCGRoom*> FloorTwo;
	for (const FInteriorPCGRoom& Room : Result.Rooms)
	{
		if (Room.FloorIndex == 1) FloorOne.Add(&Room);
		if (Room.FloorIndex == 2) FloorTwo.Add(&Room);
	}
	TestEqual(TEXT("Identical repeat floors have the same room count"), FloorOne.Num(), FloorTwo.Num());
	for (int32 Index = 0; Index < FMath::Min(FloorOne.Num(), FloorTwo.Num()); ++Index)
	{
		TestEqual(TEXT("Repeat floor room GridMin matches"), FloorOne[Index]->GridMin, FloorTwo[Index]->GridMin);
		TestEqual(TEXT("Repeat floor room GridMax matches"), FloorOne[Index]->GridMax, FloorTwo[Index]->GridMax);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteriorPCGSemanticInteriorTest, "InteriorPCG.Generation.SemanticInteriorWithoutMeshes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInteriorPCGSemanticInteriorTest::RunTest(const FString& Parameters)
{
	using namespace InteriorPCG::Tests;
	UInteriorPCGGenerationProfile* Profile = MakeProfile();
	Profile->InteriorRules = NewObject<UInteriorPCGInteriorRuleSet>(Profile);
	Profile->InteriorProps = NewObject<UInteriorPCGPropSet>(Profile);

	FInteriorPCGPropDefinition& Desk = Profile->InteriorProps->Props.Emplace_GetRef();
	Desk.PropType = EInteriorPCGPropType::Desk;
	Desk.AnchorType = EInteriorPCGAnchorType::RoomCenter;
	Desk.FootprintSize = FVector2D(120.0, 70.0);
	Desk.FrontClearance = 40.0f;

	FInteriorPCGFunctionalSetDefinition& WorkSet = Profile->InteriorRules->FunctionalSets.Emplace_GetRef();
	WorkSet.SetID = TEXT("OfficeWorkSet");
	FInteriorPCGFunctionalSetMember& DeskMember = WorkSet.Members.Emplace_GetRef();
	DeskMember.PropType = EInteriorPCGPropType::Desk;
	DeskMember.AnchorOverride = EInteriorPCGAnchorType::RoomCenter;
	DeskMember.bRequired = true;

	FInteriorPCGGenerationOptions Options = MakeOptions();
	Options.bGenerateStructure = false;
	Options.bGenerateInteriors = true;
	FInteriorPCGGenerationResult Result;
	TestTrue(TEXT("Interior generation succeeds"), UInteriorPCGGenerationLibrary::Generate(Profile, Options, Result));

	int32 DeskSignals = 0;
	for (const FInteriorPCGPlacement& Placement : Result.Placements)
	{
		if (Placement.PropType == EInteriorPCGPropType::Desk)
		{
			++DeskSignals;
			TestTrue(TEXT("Semantic placement remains valid without a mesh"), Placement.StaticMesh == nullptr);
			TestEqual(TEXT("Functional set ID is preserved"), Placement.SetID, FName(TEXT("OfficeWorkSet")));
		}
	}
	TestTrue(TEXT("At least one semantic desk signal was emitted"), DeskSignals > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteriorPCGLT1StructureTest, "InteriorPCG.Generation.PostABundleLT1Structure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInteriorPCGLT1StructureTest::RunTest(const FString& Parameters)
{
	using namespace InteriorPCG::Tests;
	UInteriorPCGGenerationProfile* Profile = MakeProfile();
	Profile->BuildingRules->CellSize = 900.0;
	Profile->BuildingRules->FloorHeight = 500.0;
	Profile->BuildingRules->CorridorWidthInCells = 1;
	Profile->BuildingRules->MinimumRoomLengthInCells = 1;
	Profile->BuildingRules->MaximumRoomLengthInCells = 2;
	Profile->BuildingRules->VerticalCores[0].SizeInCells = FIntPoint(1, 1);
	Profile->BuildingRules->bGenerateCeilingTiles = false;
	Profile->BuildingRules->bGenerateRoofTiles = true;
	Profile->BuildingRules->RoofDecorationCount = 1;
	Profile->BuildingRules->bUseFloorOpeningsAtVerticalCores = true;
	Profile->BuildingRules->bUseDedicatedExteriorCorners = true;
	Profile->BuildingRules->ExteriorCornerSpanInCells = 1;
	Profile->BuildingRules->MainEntranceSide = EInteriorPCGEntranceSide::East;
	FInteriorPCGExteriorEntranceDefinition& SecondaryEntrance = Profile->BuildingRules->AdditionalEntrances.Emplace_GetRef();
	SecondaryEntrance.Side = EInteriorPCGEntranceSide::West;
	SecondaryEntrance.NormalizedPosition = 0.0;

	FInteriorPCGGenerationOptions Options;
	Options.Footprint = FVector2D(1800.0, 2700.0);
	Options.NumFloors = 4;
	Options.BuildingSeed = 555011868;
	Options.bGenerateStructure = true;
	Options.bGenerateInteriors = false;

	FInteriorPCGGenerationResult Result;
	TestTrue(TEXT("The reference 2x3 by four-floor envelope is accepted"),
		UInteriorPCGGenerationLibrary::Generate(Profile, Options, Result));

	TMap<EInteriorPCGModuleType, int32> Counts;
	for (const FInteriorPCGPlacement& Placement : Result.Placements)
	{
		Counts.FindOrAdd(Placement.ModuleType)++;
	}
	TestEqual(TEXT("Every floor grid cell emits a slab or stair opening"),
		Counts.FindRef(EInteriorPCGModuleType::Floor) + Counts.FindRef(EInteriorPCGModuleType::FloorOpening), 24);
	TestEqual(TEXT("The roof caps all six cells"), Counts.FindRef(EInteriorPCGModuleType::Roof), 6);
	TestEqual(TEXT("The reference roof receives one vent signal"), Counts.FindRef(EInteriorPCGModuleType::RoofDecoration), 1);
	TestEqual(TEXT("Four dedicated corners exist on every floor"), Counts.FindRef(EInteriorPCGModuleType::ExteriorCorner), 16);
	TestEqual(TEXT("Four floors require three stair flights"), Counts.FindRef(EInteriorPCGModuleType::Stair), 3);
	TestEqual(TEXT("The LT1 floor slab doubles as the ceiling"), Counts.FindRef(EInteriorPCGModuleType::Ceiling), 0);
	int32 ExteriorEntrances = 0;
	for (const FInteriorPCGPortal& Portal : Result.Portals)
	{
		if (Portal.ModuleType == EInteriorPCGModuleType::Door && Portal.RoomID == INDEX_NONE)
		{
			++ExteriorEntrances;
		}
	}
	TestEqual(TEXT("Both opposite-side reference entrances are emitted"), ExteriorEntrances, 2);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
