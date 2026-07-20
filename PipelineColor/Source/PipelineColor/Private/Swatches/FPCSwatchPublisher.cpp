// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Swatches/FPCSwatchPublisher.h"

#include "Appearance/FPCFluidRoster.h"
#include "Core/FPCWorldGate.h"
#include "FGCustomizationRecipe.h"
#include "FGFactoryColoringTypes.h"
#include "FGRecipeManager.h"
#include "Patching/NativeHookManager.h"
#include "PipelineColorLog.h"
#include "PipelineColorRootInstanceModule.h"
#include "Store/APCSwatchStoreSubsystem.h"

namespace {
bool GForceRecipeHookRegistered = false;

void AppendPcCustomizationRecipes(TArray<TSubclassOf<UFGCustomizationRecipe>>& Out) {
  FPCFluidRoster::AppendAllRecipeClasses(Out);
}

UFGFactoryCustomizationCollection* LoadSwatchCollectionCDO() {
  const FSoftClassPath Path(
      TEXT("/Game/FactoryGame/Buildable/-Shared/Customization/Swatches/"
           "BP_CustomizationCollection_Swatches.BP_CustomizationCollection_Swatches_C"));
  UClass* CollectionClass = Path.TryLoadClass<UFGFactoryCustomizationCollection>();
  if (!CollectionClass) {
    UE_LOG(LogPipelineColor, Warning, TEXT("%s BP_CustomizationCollection_Swatches missing"),
           PIPELINECOLOR_LOG_PREFIX);
    return nullptr;
  }
  return Cast<UFGFactoryCustomizationCollection>(CollectionClass->GetDefaultObject());
}

void InjectOne(UFGFactoryCustomizationCollection* Collection,
               TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Swatch) {
  UPipelineColorRootInstanceModule::ApplyDefaultOrganization(Swatch);
  UPipelineColorRootInstanceModule::InjectSwatchIntoCollection(Collection, Swatch);
}
} // namespace

void FPCSwatchPublisher::RegisterForceRecipeHook() {
  if (GForceRecipeHookRegistered) {
    return;
  }
  GForceRecipeHookRegistered = true;

  SUBSCRIBE_METHOD_AFTER(AFGRecipeManager::GetAllAvailableCustomizationRecipes,
                         [](const AFGRecipeManager* /*Self*/,
                            TArray<TSubclassOf<UFGCustomizationRecipe>>& OutRecipes) {
                           AppendPcCustomizationRecipes(OutRecipes);
                         });

  UE_LOG(LogPipelineColor, Log, TEXT("%s GetAllAvailableCustomizationRecipes force hook"),
         PIPELINECOLOR_LOG_PREFIX);
}

void FPCSwatchPublisher::PublishForWorld(UWorld* World) {
  if (!FPCWorldGate::IsGameplayWorld(World)) {
    return;
  }

  UE_LOG(LogPipelineColor, Log, TEXT("%s PublishForWorld begin"), PIPELINECOLOR_LOG_PREFIX);

  UPipelineColorRootInstanceModule::GetOrCreatePipelineColorCategory();
  UPipelineColorRootInstanceModule::GetOrCreatePipelineColorSubCategory();

  APCSwatchStoreSubsystem* Store = APCSwatchStoreSubsystem::GetOrCreate(World);
  if (!Store) {
    UE_LOG(LogPipelineColor, Warning, TEXT("%s PublishForWorld: swatch store missing"),
           PIPELINECOLOR_LOG_PREFIX);
  } else {
    Store->RebuildMaps();
    Store->SeedMissingFromCatalog();
    Store->ForceReseedNeutralMatte();
  }

  UPipelineColorRootInstanceModule::UnlockPcSwatchesViaUnlockSubsystem(World);

  UFGFactoryCustomizationCollection* Collection = LoadSwatchCollectionCDO();
  if (!Collection) {
    return;
  }

  TArray<TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch>> Swatches;
  FPCFluidRoster::AppendAllSwatchClasses(Swatches);
  for (const TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch>& Swatch : Swatches) {
    InjectOne(Collection, Swatch);
  }

  UE_LOG(LogPipelineColor, Log, TEXT("%s menu injected (top category + CatalogKey color swatches)"),
         PIPELINECOLOR_LOG_PREFIX);
}
