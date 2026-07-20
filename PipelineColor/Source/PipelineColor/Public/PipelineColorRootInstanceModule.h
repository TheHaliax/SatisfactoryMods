// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Module/GameInstanceModule.h"
#include "PipelineColorRootInstanceModule.generated.h"

class AFGBuildable;
class AFGRecipeManager;
class UFGCustomizerCategory;
class UFGCustomizerSubCategory;
class UFGFactoryCustomizationCollection;
class UFGFactoryCustomizationDescriptor_Swatch;
class UFGRecipe;
class UWorld;

UCLASS()
class PIPELINECOLOR_API UPipelineColorRootInstanceModule : public UGameInstanceModule {
  GENERATED_BODY()

 public:
  UPipelineColorRootInstanceModule();

  virtual void DispatchLifecycleEvent(ELifecyclePhase Phase) override;

  static void UnregisterGlobalDelegates();

  static void InjectSwatchIntoCollection(
      UFGFactoryCustomizationCollection* Collection,
      TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Swatch);

  static void ApplyDefaultOrganization(
      TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Swatch);

  static TSubclassOf<UFGCustomizerCategory> GetOrCreatePipelineColorCategory();
  static TSubclassOf<UFGCustomizerSubCategory> GetOrCreatePipelineColorSubCategory();
  static void UnlockPcSwatchesViaUnlockSubsystem(UWorld* World);

 private:
  static void HandlePostLoadMap(UWorld* World);
  static void HandleBuildableBuilt(AFGBuildable* Buildable);
  static void CollectPcRecipes(TArray<TSubclassOf<UFGRecipe>>& Out);

  static FDelegateHandle PostLoadMapHandle;
  static TSubclassOf<UFGCustomizerCategory> CachedCategory;
  static TSubclassOf<UFGCustomizerSubCategory> CachedSubCategory;
};
