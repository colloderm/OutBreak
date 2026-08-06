// Copyright OutBreak. All Rights Reserved.

#include "InteriorPCGReferenceSetupCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/ActorComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/StaticMesh.h"
#include "InteriorPCGDataAssets.h"
#include "InteriorPCGGenerationLibrary.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InteriorPCGReferenceSetupCommandlet)

DEFINE_LOG_CATEGORY_STATIC(LogInteriorPCGReferenceSetup, Log, All);

namespace InteriorPCG::ReferenceSetup
{
	constexpr TCHAR ParentBlueprintPath[] = TEXT("/Game/PostABundle/Blueprints/Buildings/Parent/BP_P_Building.BP_P_Building");
	constexpr TCHAR LT1BlueprintPath[] = TEXT("/Game/PostABundle/Blueprints/Buildings/BP_Building_LT1.BP_Building_LT1");

	FString MakeObjectPath(const TCHAR* PackagePath)
	{
		return FString::Printf(TEXT("%s.%s"), PackagePath, *FPackageName::GetShortName(PackagePath));
	}

	template <typename T>
	T* LoadAsset(const TCHAR* PackagePath)
	{
		return LoadObject<T>(nullptr, *MakeObjectPath(PackagePath));
	}

	UClass* LoadBlueprintClass(const TCHAR* BlueprintObjectPath)
	{
		if (UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, BlueprintObjectPath))
		{
			return Blueprint->GeneratedClass;
		}
		return nullptr;
	}

	FInteriorPCGAssetVariant MakeMeshVariant(const FName ID, const TCHAR* PackagePath, const bool bCenterXY = true)
	{
		FInteriorPCGAssetVariant Variant;
		Variant.VariantID = ID;
		Variant.StaticMesh = LoadAsset<UStaticMesh>(PackagePath);
		Variant.Weight = 1.0f;
		Variant.bAllowInstancing = true;
		if (Variant.StaticMesh)
		{
			const FBoxSphereBounds Bounds = Variant.StaticMesh->GetBounds();
			Variant.NominalSize = Bounds.BoxExtent * 2.0;
			const double X = bCenterXY ? -Bounds.Origin.X : 0.0;
			const double Y = bCenterXY ? -Bounds.Origin.Y : 0.0;
			const double Z = Bounds.BoxExtent.Z - Bounds.Origin.Z;
			Variant.PlacementOffset.SetTranslation(FVector(X, Y, Z));
			UE_LOG(LogInteriorPCGReferenceSetup, Display, TEXT("MESH %s size=(%.1f, %.1f, %.1f) offset=(%.1f, %.1f, %.1f)"),
				PackagePath, Variant.NominalSize.X, Variant.NominalSize.Y, Variant.NominalSize.Z, X, Y, Z);
		}
		else
		{
			UE_LOG(LogInteriorPCGReferenceSetup, Error, TEXT("Missing mesh: %s"), PackagePath);
		}
		return Variant;
	}

	FInteriorPCGBuildingModuleDefinition& AddModule(UInteriorPCGBuildingModuleSet& Set,
		const EInteriorPCGModuleType Type, std::initializer_list<EInteriorPCGFloorBand> Bands)
	{
		FInteriorPCGBuildingModuleDefinition& Module = Set.Modules.Emplace_GetRef();
		Module.ModuleType = Type;
		for (const EInteriorPCGFloorBand Band : Bands) Module.FloorBands.Add(Band);
		return Module;
	}

	FInteriorPCGPropDefinition& AddProp(UInteriorPCGPropSet& Set, const EInteriorPCGPropType Type,
		const EInteriorPCGAnchorType Anchor, const EInteriorPCGLookAtMode LookAt,
		std::initializer_list<EInteriorPCGRoomType> Rooms)
	{
		FInteriorPCGPropDefinition& Prop = Set.Props.Emplace_GetRef();
		Prop.PropType = Type;
		Prop.AnchorType = Anchor;
		Prop.LookAtMode = LookAt;
		for (const EInteriorPCGRoomType Room : Rooms) Prop.AllowedRoomTypes.Add(Room);
		return Prop;
	}

	void SizePropFromFirstVariant(FInteriorPCGPropDefinition& Prop)
	{
		if (!Prop.Variants.IsEmpty())
		{
			Prop.FootprintSize = FVector2D(FMath::Max(10.0, Prop.Variants[0].NominalSize.X),
				FMath::Max(10.0, Prop.Variants[0].NominalSize.Y));
		}
	}

	void AddWeightedRoom(TArray<FInteriorPCGWeightedRoomType>& Rooms, const EInteriorPCGRoomType Type,
		const float Weight, const float MinimumArea, const bool bExterior = false)
	{
		FInteriorPCGWeightedRoomType& Entry = Rooms.Emplace_GetRef();
		Entry.RoomType = Type;
		Entry.Weight = Weight;
		Entry.MinimumAreaSquareMeters = MinimumArea;
		Entry.bRequiresExteriorWall = bExterior;
	}

	FInteriorPCGFunctionalSetMember& AddMember(FInteriorPCGFunctionalSetDefinition& Set,
		const EInteriorPCGPropType Type, const FVector& RelativeLocation, const bool bRequired = true,
		const float Chance = 1.0f)
	{
		FInteriorPCGFunctionalSetMember& Member = Set.Members.Emplace_GetRef();
		Member.PropType = Type;
		Member.RelativeTransform.SetTranslation(RelativeLocation);
		Member.bRequired = bRequired;
		Member.SpawnChance = Chance;
		return Member;
	}

	bool SaveAsset(UObject* Asset)
	{
		if (!Asset) return false;
		UPackage* Package = Asset->GetOutermost();
		Package->MarkPackageDirty();
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		SaveArgs.Error = GError;
		const bool bSaved = UPackage::SavePackage(Package, Asset, *Filename, SaveArgs);
		if (bSaved)
		{
			UE_LOG(LogInteriorPCGReferenceSetup, Display, TEXT("SAVE %s -> %s"), *Package->GetName(), *Filename);
		}
		else
		{
			UE_LOG(LogInteriorPCGReferenceSetup, Error, TEXT("SAVE FAILED %s -> %s"), *Package->GetName(), *Filename);
		}
		return bSaved;
	}

	void AuditBlueprint(const TCHAR* ObjectPath)
	{
		UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, ObjectPath);
		if (!Blueprint)
		{
			UE_LOG(LogInteriorPCGReferenceSetup, Error, TEXT("Blueprint load failed: %s"), ObjectPath);
			return;
		}

		const UClass* ParentClass = Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetSuperClass() : nullptr;
		const int32 SCSNodes = Blueprint->SimpleConstructionScript ? Blueprint->SimpleConstructionScript->GetAllNodes().Num() : 0;
		UE_LOG(LogInteriorPCGReferenceSetup, Display,
			TEXT("BLUEPRINT %s generated=%s parent=%s SCSNodes=%d functions=%d macros=%d ubergraphs=%d"), ObjectPath,
			Blueprint->GeneratedClass ? *Blueprint->GeneratedClass->GetPathName() : TEXT("None"),
			ParentClass ? *ParentClass->GetPathName() : TEXT("None"), SCSNodes,
			Blueprint->FunctionGraphs.Num(), Blueprint->MacroGraphs.Num(), Blueprint->UbergraphPages.Num());

		if (Blueprint->SimpleConstructionScript)
		{
			for (const USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
			{
				if (Node && Node->ComponentTemplate)
				{
					UE_LOG(LogInteriorPCGReferenceSetup, Display, TEXT("  SCS %s : %s"),
						*Node->GetVariableName().ToString(), *Node->ComponentTemplate->GetClass()->GetPathName());
				}
			}
		}

		if (Blueprint->GeneratedClass)
		{
			const UObject* Defaults = Blueprint->GeneratedClass->GetDefaultObject();
			for (TFieldIterator<FProperty> It(Blueprint->GeneratedClass, EFieldIterationFlags::IncludeSuper); It; ++It)
			{
				const FProperty* Property = *It;
				const UClass* OwnerClass = Property->GetOwnerClass();
				if (!OwnerClass || !OwnerClass->ClassGeneratedBy) continue;
				FString Value;
				Property->ExportTextItem_Direct(Value, Property->ContainerPtrToValuePtr<void>(Defaults), nullptr,
					const_cast<UObject*>(Defaults), PPF_None);
				if (Value.Len() > 320) Value = Value.Left(317) + TEXT("...");
				UE_LOG(LogInteriorPCGReferenceSetup, Display, TEXT("  PROPERTY %s.%s [%s] = %s"),
					*OwnerClass->GetName(), *Property->GetName(), *Property->GetClass()->GetName(), *Value);
			}
		}

		TArray<FName> Dependencies;
		IAssetRegistry::GetChecked().GetDependencies(Blueprint->GetOutermost()->GetFName(), Dependencies,
			UE::AssetRegistry::EDependencyCategory::Package);
		Dependencies.Sort(FNameLexicalLess());
		for (const FName Dependency : Dependencies)
		{
			if (Dependency.ToString().StartsWith(TEXT("/Game/PostABundle")))
			{
				UE_LOG(LogInteriorPCGReferenceSetup, Display, TEXT("  DEPENDENCY %s"), *Dependency.ToString());
			}
		}
	}

	bool PopulateAssets()
	{
		UInteriorPCGBuildingModuleSet* Modules = LoadAsset<UInteriorPCGBuildingModuleSet>(TEXT("/Game/DevB/PCG/DA_BuidlingModuleSet"));
		UInteriorPCGBuildingRuleSet* BuildingRules = LoadAsset<UInteriorPCGBuildingRuleSet>(TEXT("/Game/DevB/PCG/DA_BuildingRule"));
		UInteriorPCGGenerationProfile* Profile = LoadAsset<UInteriorPCGGenerationProfile>(TEXT("/Game/DevB/PCG/DA_GenerationProfile"));
		UInteriorPCGInteriorRuleSet* InteriorRules = LoadAsset<UInteriorPCGInteriorRuleSet>(TEXT("/Game/DevB/PCG/DA_InteriorRule"));
		UInteriorPCGPropSet* Props = LoadAsset<UInteriorPCGPropSet>(TEXT("/Game/DevB/PCG/DA_PropSet"));
		if (!Modules || !BuildingRules || !Profile || !InteriorRules || !Props)
		{
			UE_LOG(LogInteriorPCGReferenceSetup, Error, TEXT("One or more /Game/DevB/PCG destination assets are missing or have the wrong class."));
			return false;
		}

		Modules->Modify();
		Modules->Modules.Reset();
		AddModule(*Modules, EInteriorPCGModuleType::Floor, {EInteriorPCGFloorBand::Any}).Variants.Add(
			MakeMeshVariant(TEXT("LT1_Floor"), TEXT("/Game/PostABundle/Models/Buildings/LT1/SM_LT1_Floor")));
		AddModule(*Modules, EInteriorPCGModuleType::FloorOpening, {EInteriorPCGFloorBand::Any}).Variants.Add(
			MakeMeshVariant(TEXT("LT1_StairHole"), TEXT("/Game/PostABundle/Models/Buildings/LT1/SM_LT1_Floor_Hole")));
		AddModule(*Modules, EInteriorPCGModuleType::Roof, {EInteriorPCGFloorBand::Roof}).Variants.Add(
			MakeMeshVariant(TEXT("LT1_Roof"), TEXT("/Game/PostABundle/Models/Buildings/LT1/SM_LT1_Roof1")));

		auto AddFacade = [&Modules](const EInteriorPCGModuleType Type, const EInteriorPCGFloorBand Band,
			const FName ID, const TCHAR* MeshPath, const TCHAR* LayerPath = nullptr, const bool bCenterXY = true)
		{
			FInteriorPCGAssetVariant Variant = MakeMeshVariant(ID, MeshPath, bCenterXY);
			Variant.PlacementOffset.SetScale3D(FVector(1.0025));
			if (LayerPath)
			{
				if (UStaticMesh* Layer = LoadAsset<UStaticMesh>(LayerPath)) Variant.AdditionalStaticMeshes.Add(Layer);
				else UE_LOG(LogInteriorPCGReferenceSetup, Error, TEXT("Missing facade layer: %s"), LayerPath);
			}
			AddModule(*Modules, Type, {Band}).Variants.Add(MoveTemp(Variant));
		};

		AddFacade(EInteriorPCGModuleType::ExteriorCorner, EInteriorPCGFloorBand::Ground, TEXT("LT1_Corner_Ground"),
			TEXT("/Game/PostABundle/Models/Buildings/LT1/SM_LT1_Wall_C_B_L1"), nullptr, false);
		AddFacade(EInteriorPCGModuleType::ExteriorCorner, EInteriorPCGFloorBand::Middle, TEXT("LT1_Corner_Middle"),
			TEXT("/Game/PostABundle/Models/Buildings/LT1/SM_LT1_Wall_C_L1"),
			TEXT("/Game/PostABundle/Models/Buildings/LT1/SM_LT1_Wall_C_L1_W"), false);
		AddFacade(EInteriorPCGModuleType::ExteriorCorner, EInteriorPCGFloorBand::Top, TEXT("LT1_Corner_Top"),
			TEXT("/Game/PostABundle/Models/Buildings/LT1/SM_LT1_Wall_C_T_L1"), nullptr, false);
		AddFacade(EInteriorPCGModuleType::ExteriorWall, EInteriorPCGFloorBand::Ground, TEXT("LT1_Wall_Ground"),
			TEXT("/Game/PostABundle/Models/Buildings/LT1/SM_LT1_Wall_B1"));
		AddFacade(EInteriorPCGModuleType::ExteriorWall, EInteriorPCGFloorBand::Middle, TEXT("LT1_Wall_Middle"),
			TEXT("/Game/PostABundle/Models/Buildings/LT1/SM_LT1_Wall1"));
		AddFacade(EInteriorPCGModuleType::ExteriorWall, EInteriorPCGFloorBand::Top, TEXT("LT1_Wall_Top"),
			TEXT("/Game/PostABundle/Models/Buildings/LT1/SM_LT1_Wall_T1"));
		AddFacade(EInteriorPCGModuleType::InteriorWall, EInteriorPCGFloorBand::Any, TEXT("LT1_InteriorWall"),
			TEXT("/Game/PostABundle/Models/Buildings/LT1/SM_LT1_Wall1"));
		AddFacade(EInteriorPCGModuleType::Window, EInteriorPCGFloorBand::Ground, TEXT("LT1_Window_Ground"),
			TEXT("/Game/PostABundle/Models/Buildings/LT1/SM_LT1_Window1"),
			TEXT("/Game/PostABundle/Models/Buildings/LT1/SM_LT1_Window1_W"));
		AddFacade(EInteriorPCGModuleType::Window, EInteriorPCGFloorBand::Middle, TEXT("LT1_Window_Middle"),
			TEXT("/Game/PostABundle/Models/Buildings/LT1/SM_LT1_Window1"),
			TEXT("/Game/PostABundle/Models/Buildings/LT1/SM_LT1_Window1_W"));
		AddFacade(EInteriorPCGModuleType::Window, EInteriorPCGFloorBand::Top, TEXT("LT1_Window_Top"),
			TEXT("/Game/PostABundle/Models/Buildings/LT1/SM_LT1_Window_T1"));

		FInteriorPCGAssetVariant Door = MakeMeshVariant(TEXT("LT1_Door_Interactive"),
			TEXT("/Game/PostABundle/Models/Buildings/LT1/SM_LT1_Doorway1"));
		Door.PlacementOffset.SetScale3D(FVector(1.0025));
		Door.ActorClass = LoadBlueprintClass(TEXT("/Game/PostABundle/Blueprints/Environment/Doors/BP_LT1_Door1.BP_LT1_Door1"));
		Door.bInteractive = true;
		Door.bAllowInstancing = false;
		AddModule(*Modules, EInteriorPCGModuleType::Door, {EInteriorPCGFloorBand::Ground}).Variants.Add(MoveTemp(Door));
		FInteriorPCGAssetVariant UpperDoor = MakeMeshVariant(TEXT("LT1_Doorway"),
			TEXT("/Game/PostABundle/Models/Buildings/LT1/SM_LT1_Doorway1"));
		UpperDoor.PlacementOffset.SetScale3D(FVector(1.0025));
		AddModule(*Modules, EInteriorPCGModuleType::Door,
			{EInteriorPCGFloorBand::Middle, EInteriorPCGFloorBand::Top}).Variants.Add(MoveTemp(UpperDoor));

		FInteriorPCGAssetVariant Stair;
		Stair.VariantID = TEXT("LT1_Stair_2XX");
		Stair.ActorClass = LoadBlueprintClass(TEXT("/Game/PostABundle/Blueprints/Environment/Stairs/Interior/BP_Stairs_Steps1_2XX.BP_Stairs_Steps1_2XX"));
		Stair.NominalSize = FVector(900.0, 900.0, 500.0);
		Stair.bAllowInstancing = false;
		AddModule(*Modules, EInteriorPCGModuleType::Stair, {EInteriorPCGFloorBand::Any}).Variants.Add(MoveTemp(Stair));
		AddModule(*Modules, EInteriorPCGModuleType::RoofDecoration, {EInteriorPCGFloorBand::Roof}).Variants.Add(
			MakeMeshVariant(TEXT("LT1_RoofVent"), TEXT("/Game/PostABundle/Models/Structure/Pipes/SM_Vent_Roof_Set1")));

		BuildingRules->Modify();
		BuildingRules->CellSize = 900.0;
		BuildingRules->FloorHeight = 500.0;
		BuildingRules->CorridorWidthInCells = 1;
		BuildingRules->MinimumRoomLengthInCells = 1;
		BuildingRules->MaximumRoomLengthInCells = 2;
		BuildingRules->RepeatFloorVariation = EInteriorPCGFloorVariationMode::Identical;
		BuildingRules->PatternLength = 1;
		BuildingRules->ExteriorWindowChance = 0.6f;
		BuildingRules->MinimumWindowSpacingInCells = 1;
		BuildingRules->MainEntranceSide = EInteriorPCGEntranceSide::East;
		BuildingRules->MainEntrancePosition = 0.5;
		BuildingRules->AdditionalEntrances.Reset();
		FInteriorPCGExteriorEntranceDefinition& WestDoor = BuildingRules->AdditionalEntrances.Emplace_GetRef();
		WestDoor.Side = EInteriorPCGEntranceSide::West;
		WestDoor.NormalizedPosition = 0.0;
		BuildingRules->bGenerateCeilingTiles = false;
		BuildingRules->bGenerateRoofTiles = true;
		BuildingRules->RoofDecorationCount = 1;
		BuildingRules->bUseFloorOpeningsAtVerticalCores = true;
		BuildingRules->bUseDedicatedExteriorCorners = true;
		BuildingRules->ExteriorCornerSpanInCells = 1;
		BuildingRules->VerticalCores.Reset();
		FInteriorPCGVerticalCoreDefinition& Core = BuildingRules->VerticalCores.Emplace_GetRef();
		Core.CoreID = TEXT("LT1_MainStair");
		Core.ModuleType = EInteriorPCGModuleType::Stair;
		Core.NormalizedPosition = FVector2D(0.5, 0.5);
		Core.SizeInCells = FIntPoint(1, 1);
		BuildingRules->FirstFloorRoomTypes.Reset();
		AddWeightedRoom(BuildingRules->FirstFloorRoomTypes, EInteriorPCGRoomType::Living, 4.0f, 6.0f, true);
		AddWeightedRoom(BuildingRules->FirstFloorRoomTypes, EInteriorPCGRoomType::Kitchen, 3.0f, 6.0f, true);
		AddWeightedRoom(BuildingRules->FirstFloorRoomTypes, EInteriorPCGRoomType::Storage, 1.0f, 0.0f);
		BuildingRules->RepeatFloorRoomTypes.Reset();
		AddWeightedRoom(BuildingRules->RepeatFloorRoomTypes, EInteriorPCGRoomType::Bedroom, 4.0f, 6.0f, true);
		AddWeightedRoom(BuildingRules->RepeatFloorRoomTypes, EInteriorPCGRoomType::Living, 2.0f, 6.0f, true);
		AddWeightedRoom(BuildingRules->RepeatFloorRoomTypes, EInteriorPCGRoomType::Storage, 1.0f, 0.0f);

		Props->Modify();
		Props->Props.Reset();
		FInteriorPCGPropDefinition& Table = AddProp(*Props, EInteriorPCGPropType::Table, EInteriorPCGAnchorType::RoomCenter,
			EInteriorPCGLookAtMode::KeepAssetForward, {EInteriorPCGRoomType::Living, EInteriorPCGRoomType::Kitchen, EInteriorPCGRoomType::Meeting});
		Table.Variants.Add(MakeMeshVariant(TEXT("LT1_LivingTable"), TEXT("/Game/PostABundle/Models/Props/Furnitures/House/SM_Livingroom_Table")));
		Table.Variants.Add(MakeMeshVariant(TEXT("LT1_KitchenTable"), TEXT("/Game/PostABundle/Models/Props/Furnitures/House/SM_Kitchen_Table")));
		Table.SideClearance = 60.0f; Table.FrontClearance = 60.0f; SizePropFromFirstVariant(Table);

		FInteriorPCGPropDefinition& Sofa = AddProp(*Props, EInteriorPCGPropType::Sofa, EInteriorPCGAnchorType::Wall,
			EInteriorPCGLookAtMode::ReferenceProp, {EInteriorPCGRoomType::Living, EInteriorPCGRoomType::Lounge});
		Sofa.ReferencePropType = EInteriorPCGPropType::Television;
		Sofa.Variants.Add(MakeMeshVariant(TEXT("LT1_Couch1"), TEXT("/Game/PostABundle/Models/Props/Furnitures/House/SM_Couch1")));
		Sofa.Variants.Add(MakeMeshVariant(TEXT("LT1_Couch2"), TEXT("/Game/PostABundle/Models/Props/Furnitures/House/SM_Couch2")));
		Sofa.SideClearance = 35.0f; Sofa.FrontClearance = 120.0f; SizePropFromFirstVariant(Sofa);

		FInteriorPCGPropDefinition& TV = AddProp(*Props, EInteriorPCGPropType::Television, EInteriorPCGAnchorType::Wall,
			EInteriorPCGLookAtMode::RoomCenter, {EInteriorPCGRoomType::Living, EInteriorPCGRoomType::Lounge});
		TV.Variants.Add(MakeMeshVariant(TEXT("LT1_TV_New"), TEXT("/Game/PostABundle/Models/Props/Furnitures/House/SM_Tv_New")));
		TV.Variants.Add(MakeMeshVariant(TEXT("LT1_TV_Old"), TEXT("/Game/PostABundle/Models/Props/Furnitures/House/SM_Tv_Old")));
		TV.SideClearance = 40.0f; TV.FrontClearance = 50.0f; SizePropFromFirstVariant(TV);

		FInteriorPCGPropDefinition& Bed = AddProp(*Props, EInteriorPCGPropType::Bed, EInteriorPCGAnchorType::Wall,
			EInteriorPCGLookAtMode::RoomCenter, {EInteriorPCGRoomType::Bedroom});
		Bed.Variants.Add(MakeMeshVariant(TEXT("LT1_Bed_2X1"), TEXT("/Game/PostABundle/Models/Props/Furnitures/House/SM_Bed_2X1")));
		Bed.Variants.Add(MakeMeshVariant(TEXT("LT1_Bed_4X1"), TEXT("/Game/PostABundle/Models/Props/Furnitures/House/SM_Bed_4X1")));
		Bed.SideClearance = 45.0f; Bed.FrontClearance = 120.0f; SizePropFromFirstVariant(Bed);

		FInteriorPCGPropDefinition& Shelf = AddProp(*Props, EInteriorPCGPropType::Shelf, EInteriorPCGAnchorType::Wall,
			EInteriorPCGLookAtMode::RoomCenter, {EInteriorPCGRoomType::Storage, EInteriorPCGRoomType::Living, EInteriorPCGRoomType::Bedroom});
		Shelf.Variants.Add(MakeMeshVariant(TEXT("LT1_Shelf1"), TEXT("/Game/PostABundle/Models/Props/Furnitures/House/SM_Shelf1")));
		Shelf.Variants.Add(MakeMeshVariant(TEXT("LT1_Shelf2"), TEXT("/Game/PostABundle/Models/Props/Furnitures/House/SM_Shelf2")));
		Shelf.SideClearance = 25.0f; Shelf.FrontClearance = 90.0f; SizePropFromFirstVariant(Shelf);

		FInteriorPCGPropDefinition& Cabinet = AddProp(*Props, EInteriorPCGPropType::Cabinet, EInteriorPCGAnchorType::Wall,
			EInteriorPCGLookAtMode::RoomCenter, {EInteriorPCGRoomType::Kitchen, EInteriorPCGRoomType::Storage});
		Cabinet.Variants.Add(MakeMeshVariant(TEXT("LT1_KitchenDesk"), TEXT("/Game/PostABundle/Models/Props/Furnitures/House/SM_Kitchen_Desk_2X")));
		Cabinet.SideClearance = 20.0f; Cabinet.FrontClearance = 100.0f; SizePropFromFirstVariant(Cabinet);

		FInteriorPCGPropDefinition& Chair = AddProp(*Props, EInteriorPCGPropType::Chair, EInteriorPCGAnchorType::ReferenceProp,
			EInteriorPCGLookAtMode::ReferenceProp, {EInteriorPCGRoomType::Kitchen, EInteriorPCGRoomType::Meeting, EInteriorPCGRoomType::Office});
		Chair.ReferencePropType = EInteriorPCGPropType::Table;
		Chair.Variants.Add(MakeMeshVariant(TEXT("LT1_BasicChair"), TEXT("/Game/PostABundle/Models/Props/Furnitures/House/SM_Basic_Chair")));
		Chair.Variants.Add(MakeMeshVariant(TEXT("LT1_OfficeChair"), TEXT("/Game/PostABundle/Models/Props/Furnitures/House/SM_OfficeChair1")));
		Chair.SideClearance = 20.0f; Chair.FrontClearance = 50.0f; SizePropFromFirstVariant(Chair);

		FInteriorPCGPropDefinition& Decoration = AddProp(*Props, EInteriorPCGPropType::Decoration, EInteriorPCGAnchorType::Wall,
			EInteriorPCGLookAtMode::KeepAssetForward, {EInteriorPCGRoomType::Living, EInteriorPCGRoomType::Bedroom});
		Decoration.Variants.Add(MakeMeshVariant(TEXT("LT1_Painting1"), TEXT("/Game/PostABundle/Models/Props/Furnitures/House/SM_Painting1")));
		Decoration.Variants.Add(MakeMeshVariant(TEXT("LT1_Painting2"), TEXT("/Game/PostABundle/Models/Props/Furnitures/House/SM_Painting2")));
		Decoration.SideClearance = 15.0f; Decoration.FrontClearance = 10.0f; SizePropFromFirstVariant(Decoration);

		InteriorRules->Modify();
		InteriorRules->MainPathWidth = 100.0f;
		InteriorRules->DoorApproachDepth = 120.0f;
		InteriorRules->WindowApproachDepth = 60.0f;
		InteriorRules->PlacementAttempts = 24;
		InteriorRules->DetailSpawnChance = 0.3f;
		InteriorRules->FunctionalSets.Reset();
		FInteriorPCGFunctionalSetDefinition& LivingSet = InteriorRules->FunctionalSets.Emplace_GetRef();
		LivingSet.SetID = TEXT("LT1_LivingSet");
		LivingSet.AllowedRoomTypes = {EInteriorPCGRoomType::Living, EInteriorPCGRoomType::Lounge};
		LivingSet.Weight = 1.0f; LivingSet.MinimumAreaSquareMeters = 6.0f;
		AddMember(LivingSet, EInteriorPCGPropType::Television, FVector::ZeroVector).AnchorOverride = EInteriorPCGAnchorType::Wall;
		AddMember(LivingSet, EInteriorPCGPropType::Sofa, FVector(250.0, 0.0, 0.0)).AnchorOverride = EInteriorPCGAnchorType::Wall;
		AddMember(LivingSet, EInteriorPCGPropType::Table, FVector(100.0, 0.0, 0.0), false, 0.75f).AnchorOverride = EInteriorPCGAnchorType::RoomCenter;
		AddMember(LivingSet, EInteriorPCGPropType::Decoration, FVector::ZeroVector, false, 0.5f).AnchorOverride = EInteriorPCGAnchorType::Wall;

		FInteriorPCGFunctionalSetDefinition& KitchenSet = InteriorRules->FunctionalSets.Emplace_GetRef();
		KitchenSet.SetID = TEXT("LT1_KitchenSet");
		KitchenSet.AllowedRoomTypes = {EInteriorPCGRoomType::Kitchen};
		KitchenSet.Weight = 1.0f; KitchenSet.MinimumAreaSquareMeters = 6.0f;
		AddMember(KitchenSet, EInteriorPCGPropType::Cabinet, FVector::ZeroVector).AnchorOverride = EInteriorPCGAnchorType::Wall;
		AddMember(KitchenSet, EInteriorPCGPropType::Table, FVector::ZeroVector, false, 0.8f).AnchorOverride = EInteriorPCGAnchorType::RoomCenter;
		AddMember(KitchenSet, EInteriorPCGPropType::Chair, FVector(130.0, 0.0, 0.0), false, 0.7f).AnchorOverride = EInteriorPCGAnchorType::ReferenceProp;
		AddMember(KitchenSet, EInteriorPCGPropType::Chair, FVector(-130.0, 0.0, 0.0), false, 0.7f).AnchorOverride = EInteriorPCGAnchorType::ReferenceProp;

		FInteriorPCGFunctionalSetDefinition& BedroomSet = InteriorRules->FunctionalSets.Emplace_GetRef();
		BedroomSet.SetID = TEXT("LT1_BedroomSet");
		BedroomSet.AllowedRoomTypes = {EInteriorPCGRoomType::Bedroom};
		BedroomSet.Weight = 1.0f; BedroomSet.MinimumAreaSquareMeters = 6.0f;
		AddMember(BedroomSet, EInteriorPCGPropType::Bed, FVector::ZeroVector).AnchorOverride = EInteriorPCGAnchorType::Wall;
		AddMember(BedroomSet, EInteriorPCGPropType::Shelf, FVector::ZeroVector, false, 0.65f).AnchorOverride = EInteriorPCGAnchorType::Wall;
		AddMember(BedroomSet, EInteriorPCGPropType::Decoration, FVector::ZeroVector, false, 0.45f).AnchorOverride = EInteriorPCGAnchorType::Wall;

		FInteriorPCGFunctionalSetDefinition& StorageSet = InteriorRules->FunctionalSets.Emplace_GetRef();
		StorageSet.SetID = TEXT("LT1_StorageSet");
		StorageSet.AllowedRoomTypes = {EInteriorPCGRoomType::Storage};
		StorageSet.Weight = 1.0f;
		AddMember(StorageSet, EInteriorPCGPropType::Shelf, FVector::ZeroVector).AnchorOverride = EInteriorPCGAnchorType::Wall;
		AddMember(StorageSet, EInteriorPCGPropType::Cabinet, FVector::ZeroVector, false, 0.6f).AnchorOverride = EInteriorPCGAnchorType::Wall;

		Profile->Modify();
		Profile->BuildingRules = BuildingRules;
		Profile->InteriorRules = InteriorRules;
		Profile->BuildingModules = Modules;
		Profile->InteriorProps = Props;

		return SaveAsset(Modules) && SaveAsset(BuildingRules) && SaveAsset(InteriorRules) && SaveAsset(Props) && SaveAsset(Profile);
	}

	bool VerifyProfile()
	{
		UInteriorPCGGenerationProfile* Profile = LoadAsset<UInteriorPCGGenerationProfile>(TEXT("/Game/DevB/PCG/DA_GenerationProfile"));
		if (!Profile) return false;
		FInteriorPCGGenerationOptions Options;
		Options.Footprint = FVector2D(1800.0, 2700.0);
		Options.NumFloors = 4;
		Options.BuildingSeed = 555011868;
		Options.bGenerateStructure = true;
		Options.bGenerateInteriors = true;
		FInteriorPCGGenerationResult Result;
		const bool bGenerated = UInteriorPCGGenerationLibrary::Generate(Profile, Options, Result);
		TMap<EInteriorPCGModuleType, int32> ModuleCounts;
		int32 InteriorCount = 0;
		for (const FInteriorPCGPlacement& Placement : Result.Placements)
		{
			ModuleCounts.FindOrAdd(Placement.ModuleType)++;
			if (Placement.Kind == EInteriorPCGPlacementKind::Interior) ++InteriorCount;
		}
		UE_LOG(LogInteriorPCGReferenceSetup, Display,
			TEXT("VERIFY success=%s hash=%d rooms=%d portals=%d placements=%d floor=%d opening=%d roof=%d corner=%d stair=%d interior=%d warnings=%d"),
			bGenerated ? TEXT("true") : TEXT("false"), Result.LayoutHash, Result.Rooms.Num(), Result.Portals.Num(), Result.Placements.Num(),
			ModuleCounts.FindRef(EInteriorPCGModuleType::Floor), ModuleCounts.FindRef(EInteriorPCGModuleType::FloorOpening),
			ModuleCounts.FindRef(EInteriorPCGModuleType::Roof), ModuleCounts.FindRef(EInteriorPCGModuleType::ExteriorCorner),
			ModuleCounts.FindRef(EInteriorPCGModuleType::Stair),
			InteriorCount,
			Result.Warnings.Num());
		for (const FString& Warning : Result.Warnings) UE_LOG(LogInteriorPCGReferenceSetup, Warning, TEXT("VERIFY WARNING %s"), *Warning);
		return bGenerated && ModuleCounts.FindRef(EInteriorPCGModuleType::Roof) == 6 &&
			ModuleCounts.FindRef(EInteriorPCGModuleType::ExteriorCorner) == 16 &&
			ModuleCounts.FindRef(EInteriorPCGModuleType::Stair) == 3 && InteriorCount > 0;
	}
}

UInteriorPCGReferenceSetupCommandlet::UInteriorPCGReferenceSetupCommandlet()
{
	IsClient = false;
	IsEditor = true;
	LogToConsole = true;
	ShowErrorCount = true;
}

int32 UInteriorPCGReferenceSetupCommandlet::Main(const FString& Params)
{
	using namespace InteriorPCG::ReferenceSetup;
	UE_LOG(LogInteriorPCGReferenceSetup, Display, TEXT("Starting PostABundle LT1 audit and InteriorPCG example setup."));
	AuditBlueprint(ParentBlueprintPath);
	AuditBlueprint(LT1BlueprintPath);
	if (!PopulateAssets()) return 2;
	if (!VerifyProfile()) return 3;
	UE_LOG(LogInteriorPCGReferenceSetup, Display, TEXT("InteriorPCG reference setup completed successfully."));
	return 0;
}
