// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Swatches/FPCSwatchPublisher.h"

#include "Appearance/FPCFluidRoster.h"
#include "Appearance/FPCMetallicFinishPool.h"
#include "Core/FPCWorldGate.h"
#include "FGCustomizationRecipe.h"
#include "FGFactoryColoringTypes.h"
#include "FGRecipe.h"
#include "FGRecipeManager.h"
#include "Patching/NativeHookManager.h"
#include "PipelineColorLog.h"
#include "PipelineColorRootInstanceModule.h"
#include "Registry/ModContentRegistry.h"
#include "Session/UPCWorldSubsystem.h"
#include "Store/APCSwatchStoreSubsystem.h"
#include "UObject/SoftObjectPath.h"

namespace {
bool GForceRecipeHookRegistered = false;

void AppendPcCustomizationRecipes(const AFGRecipeManager* Self,
                                  TArray<TSubclassOf<UFGCustomizationRecipe>>& OutRecipes) {
  FPCFluidRoster::AppendAlwaysRecipeClasses(OutRecipes);
  if (!Self) {
    return;
  }
  if (UPCWorldSubsystem* Sys = UPCWorldSubsystem::Get(Self->GetWorld())) {
    Sys->AppendAvailableModRecipes(OutRecipes);
  }
}

// SFP CleanManager yeets recipes missing from ModContentRegistry. Badge ours first.
void RegisterPcRecipesWithContentRegistry(UWorld* World) {
  UModContentRegistry* Registry = UModContentRegistry::Get(World);
  if (!Registry) {
    UE_LOG(LogPipelineColor, Warning, TEXT("%s ModContentRegistry missing — SFP may scrub"),
           PIPELINECOLOR_LOG_PREFIX);
    return;
  }

  TArray<TSubclassOf<UFGCustomizationRecipe>> CustomRecipes;
  FPCFluidRoster::AppendAlwaysRecipeClasses(CustomRecipes);
  if (UPCWorldSubsystem* Sys = UPCWorldSubsystem::Get(World)) {
    Sys->AppendAvailableModRecipes(CustomRecipes);
  }

  static const FName ModRef(TEXT("PipelineColor"));
  int32 Registered = 0;
  for (const TSubclassOf<UFGCustomizationRecipe>& Recipe : CustomRecipes) {
    if (!Recipe) {
      continue;
    }
    Registry->RegisterRecipe(ModRef, TSubclassOf<UFGRecipe>(Recipe.Get()));
    ++Registered;
  }

  UE_LOG(LogPipelineColor, Log, TEXT("%s ModContentRegistry recipes=%d"), PIPELINECOLOR_LOG_PREFIX,
         Registered);
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

void InjectOne(UPipelineColorRootInstanceModule* Root,
               UFGFactoryCustomizationCollection* Collection,
               TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Swatch,
               EPCFluidRosterSection Section) {
  UPipelineColorRootInstanceModule::ApplyOrganization(Root, Swatch, Section);
  UPipelineColorRootInstanceModule::InjectSwatchIntoCollection(Collection, Swatch);
}

EPCFluidRosterSection
SectionForSwatch(TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Swatch) {
  for (const FPCFluidRosterRow& Row : FPCFluidRoster::FluidRows()) {
    if (Row.SwatchClass == Swatch) {
      return Row.Section;
    }
  }
  return EPCFluidRosterSection::Default;
}
} // namespace

void FPCSwatchPublisher::RegisterForceRecipeHook() {
  if (GForceRecipeHookRegistered) {
    return;
  }
  GForceRecipeHookRegistered = true;

  SUBSCRIBE_METHOD_AFTER(
      AFGRecipeManager::GetAllAvailableCustomizationRecipes,
      [](const AFGRecipeManager* Self, TArray<TSubclassOf<UFGCustomizationRecipe>>& OutRecipes) {
        AppendPcCustomizationRecipes(Self, OutRecipes);
      });

  UE_LOG(LogPipelineColor, Log, TEXT("%s GetAllAvailableCustomizationRecipes force hook"),
         PIPELINECOLOR_LOG_PREFIX);
}

void FPCSwatchPublisher::PublishForWorld(UWorld* World) {
  if (!FPCWorldGate::IsGameplayWorld(World)) {
    return;
  }
  // Dedicated / listen authority only. Never ClassGen / seed / CDO inject on NM_Client.
  if (World->GetNetMode() == NM_Client) {
    return;
  }

  static TWeakObjectPtr<UWorld> GPublishedWorld;
  if (GPublishedWorld.Get() == World) {
    UE_LOG(LogPipelineColor, Verbose, TEXT("%s PublishForWorld skip (already published)"),
           PIPELINECOLOR_LOG_PREFIX);
    return;
  }
  GPublishedWorld = World;

  UE_LOG(LogPipelineColor, Log, TEXT("%s PublishForWorld begin"), PIPELINECOLOR_LOG_PREFIX);

  UPipelineColorRootInstanceModule* Root = UPipelineColorRootInstanceModule::Find(World);
  if (!Root) {
    UE_LOG(LogPipelineColor, Error, TEXT("%s PublishForWorld: root module missing"),
           PIPELINECOLOR_LOG_PREFIX);
    return;
  }

  FPCMetallicFinishPool::EnsureCreated(Root);
  UPipelineColorRootInstanceModule::GetOrCreatePipelineColorCategory(Root);
  UPipelineColorRootInstanceModule::GetOrCreatePipelineColorSubCategory(Root);

  UPCWorldSubsystem* Sys = UPCWorldSubsystem::Get(World);
  if (Sys) {
    Sys->RebuildModAvailability();
    if (Sys->HasAvailableSection(EPCFluidRosterSection::Sfp)) {
      UPipelineColorRootInstanceModule::GetOrCreateSatisfactoryPlusSubCategory(Root);
    }
    if (Sys->HasAvailableSection(EPCFluidRosterSection::Rp)) {
      UPipelineColorRootInstanceModule::GetOrCreateRefinedPowerSubCategory(Root);
    }
  }

  APCSwatchStoreSubsystem* Store = APCSwatchStoreSubsystem::GetOrCreate(World);
  if (!Store) {
    UE_LOG(LogPipelineColor, Warning, TEXT("%s PublishForWorld: swatch store missing"),
           PIPELINECOLOR_LOG_PREFIX);
  } else if (Store->HasAuthority()) {
    Store->RebuildMaps();
    Store->SeedMissingFromCatalog();
    // Do not ForceReseedNeutralMatte every publish — rewrite only when Neutral absent.
    FPCSwatchEntry Neutral;
    if (!Store->TryGet(FName(TEXT("Neutral")), Neutral) || Neutral.PaintFinishPath.IsEmpty()) {
      Store->ForceReseedNeutralMatte();
    }
  } else {
    UE_LOG(LogPipelineColor, Warning, TEXT("%s PublishForWorld: store lacks authority"),
           PIPELINECOLOR_LOG_PREFIX);
  }

  RegisterPcRecipesWithContentRegistry(World);
  UPipelineColorRootInstanceModule::UnlockPcSwatchesViaUnlockSubsystem(World);

  UFGFactoryCustomizationCollection* Collection = LoadSwatchCollectionCDO();
  if (!Collection) {
    return;
  }

  TArray<TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch>> Swatches;
  FPCFluidRoster::AppendAlwaysSwatchClasses(Swatches);
  if (Sys) {
    Sys->AppendAvailableModSwatches(Swatches);
  }
  for (const TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch>& Swatch : Swatches) {
    InjectOne(Root, Collection, Swatch, SectionForSwatch(Swatch));
  }

  UE_LOG(LogPipelineColor, Log, TEXT("%s menu injected (Default + available mod swatches)"),
         PIPELINECOLOR_LOG_PREFIX);
}
