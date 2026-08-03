#include "InteriorPCGSetupCommandlet.h"

#include "InteriorPCGPreset.h"
#include "PCGInteriorGeneratorSettings.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "PCGCommon.h"
#include "PCGGraph.h"
#include "PCGNode.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogInteriorPCGSetup, Log, All);

namespace InteriorPCGSetupPrivate
{
	bool SaveAsset(UPackage* Package, UObject* Asset)
	{
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		return UPackage::SavePackage(Package, Asset, *Filename, SaveArgs);
	}
}

UInteriorPCGSetupCommandlet::UInteriorPCGSetupCommandlet()
{
	IsClient = false;
	IsEditor = true;
	LogToConsole = true;
	ShowErrorCount = true;
}

int32 UInteriorPCGSetupCommandlet::Main(const FString& Params)
{
	bool bSuccess = true;

	const FString GraphPackageName = TEXT("/Game/InteriorPCG/PCG_InteriorGenerator");
	UPCGGraph* Graph = FPackageName::DoesPackageExist(GraphPackageName)
		? LoadObject<UPCGGraph>(nullptr, TEXT("/Game/InteriorPCG/PCG_InteriorGenerator.PCG_InteriorGenerator"))
		: nullptr;
	if (!Graph)
	{
		UPackage* GraphPackage = CreatePackage(*GraphPackageName);
		Graph = NewObject<UPCGGraph>(GraphPackage, TEXT("PCG_InteriorGenerator"), RF_Public | RF_Standalone | RF_Transactional);
		UPCGInteriorGeneratorSettings* NodeSettings = nullptr;
		UPCGNode* GeneratorNode = Graph->AddNodeOfType(NodeSettings);
		if (GeneratorNode)
		{
			GeneratorNode->PositionX = 200;
			Graph->GetOutputNode()->PositionX = 400;
			Graph->AddEdge(Graph->GetInputNode(), PCGPinConstants::DefaultInputLabel, GeneratorNode, PCGPinConstants::DefaultInputLabel);
			Graph->AddEdge(GeneratorNode, PCGPinConstants::DefaultOutputLabel, Graph->GetOutputNode(), PCGPinConstants::DefaultOutputLabel);
		}
		FAssetRegistryModule::AssetCreated(Graph);
		GraphPackage->MarkPackageDirty();
		bSuccess &= InteriorPCGSetupPrivate::SaveAsset(GraphPackage, Graph);
	}

	const FString PresetPackageName = TEXT("/Game/InteriorPCG/Presets/PCGPreset_EmptyExample");
	UInteriorPCGPreset* Preset = FPackageName::DoesPackageExist(PresetPackageName)
		? LoadObject<UInteriorPCGPreset>(nullptr, TEXT("/Game/InteriorPCG/Presets/PCGPreset_EmptyExample.PCGPreset_EmptyExample"))
		: nullptr;
	if (!Preset)
	{
		UPackage* PresetPackage = CreatePackage(*PresetPackageName);
		Preset = NewObject<UInteriorPCGPreset>(PresetPackage, TEXT("PCGPreset_EmptyExample"), RF_Public | RF_Standalone | RF_Transactional);
		Preset->Description = NSLOCTEXT("InteriorPCG", "EmptyExamplePresetDescription", "Empty example. Save or update from an edited Interior PCG Volume to populate it.");
		FAssetRegistryModule::AssetCreated(Preset);
		PresetPackage->MarkPackageDirty();
		bSuccess &= InteriorPCGSetupPrivate::SaveAsset(PresetPackage, Preset);
	}

	UE_LOG(LogInteriorPCGSetup, Display, TEXT("Interior PCG setup %s. Graph=%s Preset=%s"), bSuccess ? TEXT("succeeded") : TEXT("failed"), *GetPathNameSafe(Graph), *GetPathNameSafe(Preset));
	return bSuccess ? 0 : 1;
}
