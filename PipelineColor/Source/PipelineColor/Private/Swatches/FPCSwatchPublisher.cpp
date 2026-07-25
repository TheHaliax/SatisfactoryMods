// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Swatches/FPCSwatchPublisher.h"

#include "Appearance/FPCMetallicFinishPool.h"
#include "Core/FPCWorldGate.h"
#include "FGCustomizationRecipe.h"
#include "FGRecipe.h"
#include "FGRecipeManager.h"
#include "Patching/NativeHookManager.h"
#include "PipelineColorLog.h"
#include "PipelineColorRootInstanceModule.h"
#include "Registry/ModContentRegistry.h"
#include "Session/UPCWorldSubsystem.h"
#include "Store/APCSwatchStoreSubsystem.h"
#include "Swatches/FPCDynamicSwatchRegistry.h"
#include "Swatches/UPCSwatchDescs.h"
#include "UObject/SoftObjectPath.h"

namespace {
bool GForceRecipeHookRegistered = false;

void AppendPcCustomizationRecipes(const AFGRecipeManager* Self,
                                  TArray<TSubclassOf<UFGCustomizationRecipe>>& OutRecipes) {
  FPCDynamicSwatchRegistry::AppendRecipeClasses(OutRecipes);
}

void RegisterPcRecipesWithContentRegistry(UWorld* World) {
  UModContentRegistry* Registry = UModContentRegistry::Get(World);
  if (!Registry) {
    UE_LOG(LogPipelineColor, Warning, TEXT("%s ModContentRegistry missing — SFP may scrub"),
           PIPELINECOLOR_LOG_PREFIX);
    return;
  }

  TArray<TSubclassOf<UFGCustomizationRecipe>> CustomRecipes;
  FPCDynamicSwatchRegistry::AppendRecipeClasses(CustomRecipes);

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
  FPCDynamicSwatchRegistry::Ensure(World, Root, /*bForceRescan=*/true);

  APCSwatchStoreSubsystem* Store = APCSwatchStoreSubsystem::GetOrCreate(World);
  if (!Store) {
    UE_LOG(LogPipelineColor, Warning, TEXT("%s PublishForWorld: swatch store missing"),
           PIPELINECOLOR_LOG_PREFIX);
  } else if (Store->HasAuthority()) {
    Store->RebuildMaps();
    FPCSwatchEntry Neutral;
    if (Store->TryGet(FName(TEXT("Neutral")), Neutral) && Neutral.PaintFinishPath.IsEmpty()) {
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

  UPipelineColorRootInstanceModule::ApplyOrganization(Root, UPCSwatchDesc_Neutral::StaticClass(),
                                                      TEXT("Default"));
  UPipelineColorRootInstanceModule::InjectSwatchIntoCollection(
      Collection, UPCSwatchDesc_Neutral::StaticClass());

  for (const FPCDynamicSwatchEntry& Entry : FPCDynamicSwatchRegistry::Entries()) {
    if (!Entry.SwatchClass) {
      continue;
    }
    UPipelineColorRootInstanceModule::ApplyOrganization(Root, Entry.SwatchClass,
                                                        Entry.OwnerFriendlyName);
    UPipelineColorRootInstanceModule::InjectSwatchIntoCollection(Collection, Entry.SwatchClass);
  }

  UPipelineColorRootInstanceModule::ApplyOrganization(Root, UPCSwatchDesc_Fallback::StaticClass(),
                                                      TEXT("Default"));
  UPipelineColorRootInstanceModule::InjectSwatchIntoCollection(
      Collection, UPCSwatchDesc_Fallback::StaticClass());

  UE_LOG(LogPipelineColor, Log, TEXT("%s menu injected (dynamic fluids)"),
         PIPELINECOLOR_LOG_PREFIX);
}
