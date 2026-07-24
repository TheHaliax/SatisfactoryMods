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
class UFGFactoryCustomizationDescriptor_PaintFinish;
class UFGFactoryCustomizationDescriptor_Swatch;
class UFGRecipe;
class UWorld;
enum class EPCFluidRosterSection : uint8;

UCLASS()
class PIPELINECOLOR_API UPipelineColorRootInstanceModule : public UGameInstanceModule {
  GENERATED_BODY()

 public:
  UPipelineColorRootInstanceModule();

  virtual void DispatchLifecycleEvent(ELifecyclePhase Phase) override;

  static void UnregisterGlobalDelegates();

  static UPipelineColorRootInstanceModule* Find(UWorld* World);

  static void
  InjectSwatchIntoCollection(UFGFactoryCustomizationCollection* Collection,
                             TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Swatch);

  static void
  ApplyDefaultOrganization(UPipelineColorRootInstanceModule* Root,
                           TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Swatch);

  static void ApplyOrganization(UPipelineColorRootInstanceModule* Root,
                                TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Swatch,
                                EPCFluidRosterSection Section);

  static TSubclassOf<UFGCustomizerCategory>
  GetOrCreatePipelineColorCategory(UPipelineColorRootInstanceModule* Root);
  static TSubclassOf<UFGCustomizerSubCategory>
  GetOrCreatePipelineColorSubCategory(UPipelineColorRootInstanceModule* Root);
  static TSubclassOf<UFGCustomizerSubCategory>
  GetOrCreateSatisfactoryPlusSubCategory(UPipelineColorRootInstanceModule* Root);
  static TSubclassOf<UFGCustomizerSubCategory>
  GetOrCreateRefinedPowerSubCategory(UPipelineColorRootInstanceModule* Root);
  static void UnlockPcSwatchesViaUnlockSubsystem(UWorld* World);

  /** ClassGen metallic roughness flyweights — GC roots (never stamp CDOs at apply). */
  UPROPERTY()
  TArray<TSubclassOf<UFGFactoryCustomizationDescriptor_PaintFinish>> MetallicFinishBuckets;

  UPROPERTY()
  TSubclassOf<UFGFactoryCustomizationDescriptor_PaintFinish> MetallicNeutralFinish;

  UPROPERTY()
  TSubclassOf<UFGCustomizerCategory> CachedCategory;

  UPROPERTY()
  TSubclassOf<UFGCustomizerSubCategory> CachedSubCategory;

  UPROPERTY()
  TSubclassOf<UFGCustomizerSubCategory> CachedSfpSubCategory;

  UPROPERTY()
  TSubclassOf<UFGCustomizerSubCategory> CachedRpSubCategory;

  bool bMetallicFinishPoolReady = false;

 private:
  static void HandlePostLoadMap(UWorld* World);
  static void HandleBuildableBuilt(AFGBuildable* Buildable);
  static void CollectPcRecipes(UWorld* World, TArray<TSubclassOf<UFGRecipe>>& Out);
  static TSubclassOf<UFGCustomizerSubCategory>
  GetOrCreateNamedSubCategory(UPipelineColorRootInstanceModule* Root,
                              TSubclassOf<UFGCustomizerSubCategory>& Cache, const TCHAR* ClassName,
                              const TCHAR* DisplayName, float MenuPriority);

  static FDelegateHandle PostLoadMapHandle;
};
