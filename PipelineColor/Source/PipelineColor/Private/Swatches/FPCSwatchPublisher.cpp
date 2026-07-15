// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Swatches/FPCSwatchPublisher.h"

#include "Core/FPCWorldGate.h"
#include "FGCustomizationRecipe.h"
#include "FGFactoryColoringTypes.h"
#include "FGRecipeManager.h"
#include "Patching/NativeHookManager.h"
#include "PipelineColorLog.h"
#include "PipelineColorRootInstanceModule.h"
#include "Store/APCSwatchStoreSubsystem.h"
#include "Swatches/UPCSwatchDescs.h"
#include "Swatches/UPCSwatchRecipes.h"

namespace
{
bool GForceRecipeHookRegistered = false;

void AppendPcCustomizationRecipes(
	TArray<TSubclassOf<UFGCustomizationRecipe>>& Out)
{
	static const TSubclassOf<UFGCustomizationRecipe> Recipes[] = {
		UPCSwatchRecipe_Neutral::StaticClass(),
		UPCSwatchRecipe_Water::StaticClass(),
		UPCSwatchRecipe_CrudeOil::StaticClass(),
		UPCSwatchRecipe_HeavyOilResidue::StaticClass(),
		UPCSwatchRecipe_Fuel::StaticClass(),
		UPCSwatchRecipe_Turbofuel::StaticClass(),
		UPCSwatchRecipe_LiquidBiofuel::StaticClass(),
		UPCSwatchRecipe_AluminaSolution::StaticClass(),
		UPCSwatchRecipe_SulfuricAcid::StaticClass(),
		UPCSwatchRecipe_DissolvedSilica::StaticClass(),
		UPCSwatchRecipe_NitricAcid::StaticClass(),
		UPCSwatchRecipe_DarkMatterResidue::StaticClass(),
		UPCSwatchRecipe_ExcitedPhotonicMatter::StaticClass(),
		UPCSwatchRecipe_IonizedFuel::StaticClass(),
		UPCSwatchRecipe_RocketFuel::StaticClass(),
		UPCSwatchRecipe_NitrogenGas::StaticClass(),
		UPCSwatchRecipe_Fallback::StaticClass(),
	};

	for (const TSubclassOf<UFGCustomizationRecipe>& Recipe : Recipes)
	{
		Out.AddUnique(Recipe);
	}
}

UFGFactoryCustomizationCollection* LoadSwatchCollectionCDO()
{
	const FSoftClassPath Path(TEXT(
		"/Game/FactoryGame/Buildable/-Shared/Customization/Swatches/"
		"BP_CustomizationCollection_Swatches.BP_CustomizationCollection_Swatches_C"));
	UClass* CollectionClass = Path.TryLoadClass<UFGFactoryCustomizationCollection>();
	if (!CollectionClass)
	{
		UE_LOG(LogPipelineColor, Warning,
			TEXT("%s BP_CustomizationCollection_Swatches missing"),
			PIPELINECOLOR_LOG_PREFIX);
		return nullptr;
	}
	return Cast<UFGFactoryCustomizationCollection>(CollectionClass->GetDefaultObject());
}

void InjectOne(
	UFGFactoryCustomizationCollection* Collection,
	TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Swatch)
{
	UPipelineColorRootInstanceModule::ApplyDefaultOrganization(Swatch);
	UPipelineColorRootInstanceModule::InjectSwatchIntoCollection(Collection, Swatch);
}
} // namespace

void FPCSwatchPublisher::RegisterForceRecipeHook()
{
	if (GForceRecipeHookRegistered)
	{
		return;
	}
	GForceRecipeHookRegistered = true;

	SUBSCRIBE_METHOD_AFTER(
		AFGRecipeManager::GetAllAvailableCustomizationRecipes,
		[](const AFGRecipeManager* /*Self*/,
			TArray<TSubclassOf<UFGCustomizationRecipe>>& OutRecipes)
		{
			AppendPcCustomizationRecipes(OutRecipes);
		});

	UE_LOG(LogPipelineColor, Log,
		TEXT("%s GetAllAvailableCustomizationRecipes force hook"),
		PIPELINECOLOR_LOG_PREFIX);
}

void FPCSwatchPublisher::PublishForWorld(UWorld* World)
{
	if (!FPCWorldGate::IsGameplayWorld(World))
	{
		return;
	}

	UE_LOG(LogPipelineColor, Log, TEXT("%s PublishForWorld begin"),
		PIPELINECOLOR_LOG_PREFIX);

	UPipelineColorRootInstanceModule::GetOrCreatePipelineColorCategory();
	UPipelineColorRootInstanceModule::GetOrCreatePipelineColorSubCategory();

	APCSwatchStoreSubsystem* Store = APCSwatchStoreSubsystem::GetOrCreate(World);
	if (Store)
	{
		Store->RebuildMaps();
		Store->SeedMissingFromCatalog();
		Store->ForceReseedNeutralMatte();
	}

	UPipelineColorRootInstanceModule::UnlockPcSwatchesViaUnlockSubsystem(World);

	UFGFactoryCustomizationCollection* Collection = LoadSwatchCollectionCDO();
	if (!Collection)
	{
		return;
	}

	InjectOne(Collection, UPCSwatchDesc_Neutral::StaticClass());
	InjectOne(Collection, UPCSwatchDesc_Water::StaticClass());
	InjectOne(Collection, UPCSwatchDesc_CrudeOil::StaticClass());
	InjectOne(Collection, UPCSwatchDesc_HeavyOilResidue::StaticClass());
	InjectOne(Collection, UPCSwatchDesc_Fuel::StaticClass());
	InjectOne(Collection, UPCSwatchDesc_Turbofuel::StaticClass());
	InjectOne(Collection, UPCSwatchDesc_LiquidBiofuel::StaticClass());
	InjectOne(Collection, UPCSwatchDesc_AluminaSolution::StaticClass());
	InjectOne(Collection, UPCSwatchDesc_SulfuricAcid::StaticClass());
	InjectOne(Collection, UPCSwatchDesc_DissolvedSilica::StaticClass());
	InjectOne(Collection, UPCSwatchDesc_NitricAcid::StaticClass());
	InjectOne(Collection, UPCSwatchDesc_DarkMatterResidue::StaticClass());
	InjectOne(Collection, UPCSwatchDesc_ExcitedPhotonicMatter::StaticClass());
	InjectOne(Collection, UPCSwatchDesc_IonizedFuel::StaticClass());
	InjectOne(Collection, UPCSwatchDesc_RocketFuel::StaticClass());
	InjectOne(Collection, UPCSwatchDesc_NitrogenGas::StaticClass());
	InjectOne(Collection, UPCSwatchDesc_Fallback::StaticClass());

	UE_LOG(LogPipelineColor, Log,
		TEXT("%s menu injected (top category + CatalogKey color swatches)"),
		PIPELINECOLOR_LOG_PREFIX);
}
