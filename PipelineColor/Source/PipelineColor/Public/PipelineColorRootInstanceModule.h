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
class UFGCustomizationRecipe;
class UFGRecipe;
class UWorld;

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

  static void ApplyOrganization(UPipelineColorRootInstanceModule* Root,
                                TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Swatch,
                                const FString& SubCategoryDisplayName);

  static TSubclassOf<UFGCustomizerCategory>
  GetOrCreatePipelineColorCategory(UPipelineColorRootInstanceModule* Root);
  static TSubclassOf<UFGCustomizerSubCategory>
  GetOrCreatePipelineColorSubCategory(UPipelineColorRootInstanceModule* Root);
  static TSubclassOf<UFGCustomizerSubCategory>
  GetOrCreateSubCategoryByDisplayName(UPipelineColorRootInstanceModule* Root,
                                      const FString& DisplayName);
  static void UnlockPcSwatchesViaUnlockSubsystem(UWorld* World);

  UPROPERTY()
  TArray<TSubclassOf<UFGFactoryCustomizationDescriptor_PaintFinish>> MetallicFinishBuckets;

  UPROPERTY()
  TSubclassOf<UFGFactoryCustomizationDescriptor_PaintFinish> MetallicNeutralFinish;

  UPROPERTY()
  TSubclassOf<UFGCustomizerCategory> CachedCategory;

  UPROPERTY()
  TSubclassOf<UFGCustomizerSubCategory> CachedSubCategory;

  UPROPERTY()
  TMap<FName, TSubclassOf<UFGCustomizerSubCategory>> CachedSubCategoriesByName;

  UPROPERTY()
  TArray<TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch>> DynamicSwatchClasses;

  UPROPERTY()
  TArray<TSubclassOf<UFGCustomizationRecipe>> DynamicRecipeClasses;

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
