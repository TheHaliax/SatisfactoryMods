// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "PipelineColorRootInstanceModule.h"

#include "Appearance/FPCFluidAppearanceCatalog.h"
#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildablePipeline.h"
#include "Buildables/FGBuildablePipelineAttachment.h"
#include "Command/PipelineColorSmlChatCommands.h"
#include "Config/FPCPipelineColorModConfig.h"
#include "Content/FPipeFluidKeyResolver.h"
#include "Content/FPipeSupportTouch.h"
#include "Core/FPCWorldGate.h"
#include "Engine/Texture2D.h"
#include "FGBuildableSubsystem.h"
#include "FGCategory.h"
#include "FGCustomizationRecipe.h"
#include "FGCustomizerCategory.h"
#include "FGCustomizerSubCategory.h"
#include "FGFactoryColoringTypes.h"
#include "FGRecipe.h"
#include "FGRecipeManager.h"
#include "FGUnlockSubsystem.h"
#include "Network/UPCChatRCO.h"
#include "Observation/FFluidAppearanceObserver.h"
#include "Patching/NativeHookManager.h"
#include "PipelineColorLog.h"
#include "Reflection/ClassGenerator.h"
#include "Session/FPCLoadAnnouncement.h"
#include "Session/UPCWorldSubsystem.h"
#include "Store/FPCSwatchSlotDispatch.h"
#include "Swatches/FPCSwatchPublisher.h"
#include "Swatches/UPCSwatchRecipes.h"
#include "Swatches/UPCSwatchSubCategory.h"
#include "TimerManager.h"
#include "UObject/SoftObjectPath.h"
#include "Unlocks/FGUnlockCustomizer.h"
#include "Unlocks/FGUnlockRecipe.h"

FDelegateHandle UPipelineColorRootInstanceModule::PostLoadMapHandle;
TSubclassOf<UFGCustomizerCategory> UPipelineColorRootInstanceModule::CachedCategory;
TSubclassOf<UFGCustomizerSubCategory> UPipelineColorRootInstanceModule::CachedSubCategory;

UPipelineColorRootInstanceModule::UPipelineColorRootInstanceModule() {
  bRootModule = true;
  RemoteCallObjects.Add(UPCChatRCO::StaticClass());
}

void UPipelineColorRootInstanceModule::UnregisterGlobalDelegates() {
  if (PostLoadMapHandle.IsValid()) {
    FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
    PostLoadMapHandle.Reset();
  }
}

void UPipelineColorRootInstanceModule::CollectPcRecipes(TArray<TSubclassOf<UFGRecipe>>& Out) {
  Out.Reset();
  Out.Add(UPCSwatchRecipe_Neutral::StaticClass());
  Out.Add(UPCSwatchRecipe_Water::StaticClass());
  Out.Add(UPCSwatchRecipe_CrudeOil::StaticClass());
  Out.Add(UPCSwatchRecipe_HeavyOilResidue::StaticClass());
  Out.Add(UPCSwatchRecipe_Fuel::StaticClass());
  Out.Add(UPCSwatchRecipe_Turbofuel::StaticClass());
  Out.Add(UPCSwatchRecipe_LiquidBiofuel::StaticClass());
  Out.Add(UPCSwatchRecipe_AluminaSolution::StaticClass());
  Out.Add(UPCSwatchRecipe_SulfuricAcid::StaticClass());
  Out.Add(UPCSwatchRecipe_DissolvedSilica::StaticClass());
  Out.Add(UPCSwatchRecipe_NitricAcid::StaticClass());
  Out.Add(UPCSwatchRecipe_DarkMatterResidue::StaticClass());
  Out.Add(UPCSwatchRecipe_ExcitedPhotonicMatter::StaticClass());
  Out.Add(UPCSwatchRecipe_IonizedFuel::StaticClass());
  Out.Add(UPCSwatchRecipe_RocketFuel::StaticClass());
  Out.Add(UPCSwatchRecipe_NitrogenGas::StaticClass());
  Out.Add(UPCSwatchRecipe_Fallback::StaticClass());
}

TSubclassOf<UFGCustomizerCategory>
UPipelineColorRootInstanceModule::GetOrCreatePipelineColorCategory() {
  if (CachedCategory) {
    return CachedCategory;
  }

  UClass* Generated = FClassGenerator::GenerateSimpleClass(TEXT("/PipelineColor"),
                                                           TEXT("PC_Category_PipelineColor"),
                                                           UFGCustomizerCategory::StaticClass());
  if (!Generated) {
    UE_LOG(LogPipelineColor, Error, TEXT("%s ClassGenerator failed for PipelineColor category"),
           PIPELINECOLOR_LOG_PREFIX);
    return nullptr;
  }

  if (UFGCategory* CDO = Cast<UFGCategory>(Generated->GetDefaultObject())) {
    CDO->mDisplayName = FText::FromString(TEXT("PipelineColor"));
    CDO->mMenuPriority = 35.f;

    const FSoftObjectPath IconPath(
        TEXT("/Game/FactoryGame/Buildable/-Shared/Customization/Patterns/Icons/"
             "IconDesc_ParkingStencil_512.IconDesc_ParkingStencil_512"));
    if (UTexture2D* Icon = Cast<UTexture2D>(IconPath.TryLoad())) {
      CDO->mCategoryIcon.SetResourceObject(Icon);
      CDO->mCategoryIcon.ImageSize =
          FVector2D(static_cast<float>(Icon->GetSizeX()), static_cast<float>(Icon->GetSizeY()));
      CDO->mCategoryIcon.DrawAs = ESlateBrushDrawType::Image;
    } else {
      UE_LOG(LogPipelineColor, Warning, TEXT("%s category: Parking stencil icon missing"),
             PIPELINECOLOR_LOG_PREFIX);
    }
  }

  CachedCategory = Generated;
  UE_LOG(LogPipelineColor, Log, TEXT("%s ClassGen top category %s"), PIPELINECOLOR_LOG_PREFIX,
         *GetNameSafe(Generated));
  return CachedCategory;
}

TSubclassOf<UFGCustomizerSubCategory>
UPipelineColorRootInstanceModule::GetOrCreatePipelineColorSubCategory() {
  if (CachedSubCategory) {
    return CachedSubCategory;
  }

  UClass* Generated = FClassGenerator::GenerateSimpleClass(
      TEXT("/PipelineColor"), TEXT("PC_SubCat_Default"), UPCSwatchSubCategory::StaticClass());
  if (!Generated) {
    UE_LOG(LogPipelineColor, Error, TEXT("%s ClassGenerator failed for PipelineColor subcategory"),
           PIPELINECOLOR_LOG_PREFIX);
    CachedSubCategory = UPCSwatchSubCategory::StaticClass();
    return CachedSubCategory;
  }

  if (UFGCategory* CDO = Cast<UFGCategory>(Generated->GetDefaultObject())) {
    CDO->mDisplayName = FText::FromString(TEXT("Default"));
    CDO->mMenuPriority = 0.f;
  }

  CachedSubCategory = Generated;
  UE_LOG(LogPipelineColor, Log, TEXT("%s ClassGen subcategory %s"), PIPELINECOLOR_LOG_PREFIX,
         *GetNameSafe(Generated));
  return CachedSubCategory;
}

void UPipelineColorRootInstanceModule::InjectSwatchIntoCollection(
    UFGFactoryCustomizationCollection* Collection,
    TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Swatch) {
  if (!IsValid(Collection) || !Swatch) {
    return;
  }

  if (!Collection->mCustomizations.Contains(Swatch)) {
    Collection->mCustomizations.Add(Swatch);
    UE_LOG(LogPipelineColor, Log, TEXT("%s customizer +%s"), PIPELINECOLOR_LOG_PREFIX,
           *GetNameSafe(Swatch));
  }
}

void UPipelineColorRootInstanceModule::ApplyDefaultOrganization(
    TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Swatch) {
  if (!Swatch) {
    return;
  }

  UFGFactoryCustomizationDescriptor_Swatch* CDO =
      Swatch->GetDefaultObject<UFGFactoryCustomizationDescriptor_Swatch>();
  if (!CDO) {
    return;
  }

  if (TSubclassOf<UFGCustomizerCategory> Category = GetOrCreatePipelineColorCategory()) {
    CDO->mCategory = Category;
  }

  if (!CDO->mIcon.IsValid()) {
    const FSoftClassPath Slot0Path(
        TEXT("/Game/FactoryGame/Buildable/-Shared/Customization/Swatches/"
             "SwatchDesc_Slot0.SwatchDesc_Slot0_C"));
    if (UClass* TemplateClass =
            Slot0Path.TryLoadClass<UFGFactoryCustomizationDescriptor_Swatch>()) {
      if (const UFGFactoryCustomizationDescriptor_Swatch* Template =
              TemplateClass->GetDefaultObject<UFGFactoryCustomizationDescriptor_Swatch>()) {
        CDO->mIcon = Template->mIcon;
        CDO->mValidBuildables = Template->mValidBuildables;
      }
    }
  }

  CDO->mSubCategories.Reset();
  CDO->mSubCategories.Add(GetOrCreatePipelineColorSubCategory());
  CDO->mMenuPriority = 10.f;
}

void UPipelineColorRootInstanceModule::UnlockPcSwatchesViaUnlockSubsystem(UWorld* World) {
  if (!IsValid(World) || World->GetNetMode() == NM_Client) {
    return;
  }

  AFGUnlockSubsystem* UnlockSys = AFGUnlockSubsystem::Get(World);
  if (!UnlockSys) {
    UE_LOG(LogPipelineColor, Warning, TEXT("%s UnlockSubsystem missing"), PIPELINECOLOR_LOG_PREFIX);
    return;
  }

  TArray<TSubclassOf<UFGRecipe>> Batch;
  CollectPcRecipes(Batch);

  const FSoftObjectPath BuildGunPath(
      TEXT("/Game/FactoryGame/Equipment/BuildGun/BP_BuildGun.BP_BuildGun_C"));
  for (const TSubclassOf<UFGRecipe>& Recipe : Batch) {
    if (!Recipe || !Recipe->IsChildOf(UFGCustomizationRecipe::StaticClass())) {
      continue;
    }
    if (UFGCustomizationRecipe* RecipeCDO =
            Cast<UFGCustomizationRecipe>(Recipe->GetDefaultObject())) {
      RecipeCDO->mProducedIn.Reset();
      RecipeCDO->mProducedIn.Add(TSoftClassPtr<UObject>(BuildGunPath));
    }
  }

  const FSoftClassPath CustomizerUnlockPath(
      TEXT("/Game/FactoryGame/Unlocks/BP_UnlockCustomizer.BP_UnlockCustomizer_C"));
  if (UClass* CustomizerUnlockClass = CustomizerUnlockPath.TryLoadClass<UFGUnlockCustomizer>()) {
    UFGUnlockCustomizer* CustomizerUnlock =
        NewObject<UFGUnlockCustomizer>(GetTransientPackage(), CustomizerUnlockClass);
    CustomizerUnlock->Apply(UnlockSys);
    UE_LOG(LogPipelineColor, Log, TEXT("%s BP_UnlockCustomizer applied"), PIPELINECOLOR_LOG_PREFIX);
  } else {
    UnlockSys->UnlockCustomizer();
    UE_LOG(LogPipelineColor, Warning,
           TEXT("%s BP_UnlockCustomizer missing — UnlockCustomizer() fallback"),
           PIPELINECOLOR_LOG_PREFIX);
  }

  const FSoftClassPath RecipeUnlockPath(
      TEXT("/Game/FactoryGame/Unlocks/BP_UnlockRecipe.BP_UnlockRecipe_C"));
  UClass* RecipeUnlockClass = RecipeUnlockPath.TryLoadClass<UFGUnlockRecipe>();
  if (!RecipeUnlockClass) {
    UE_LOG(LogPipelineColor, Error, TEXT("%s BP_UnlockRecipe missing"), PIPELINECOLOR_LOG_PREFIX);
    return;
  }

  UFGUnlockRecipe* RecipeUnlock =
      NewObject<UFGUnlockRecipe>(GetTransientPackage(), RecipeUnlockClass);
  RecipeUnlock->mRecipes = Batch;
  RecipeUnlock->Apply(UnlockSys);

  AFGRecipeManager* Recipes = AFGRecipeManager::Get(World);
  int32 Available = 0;
  int32 Ours = 0;
  if (Recipes) {
    Available = Recipes->mAvailableCustomizationRecipes.Num();
    for (const TSubclassOf<UFGRecipe>& Recipe : Batch) {
      TSubclassOf<UFGCustomizationRecipe> Cust(Recipe.Get());
      if (Cust && Recipes->IsCustomizationRecipeAvailable(Cust)) {
        ++Ours;
      }
    }
  }

  UE_LOG(LogPipelineColor, Log,
         TEXT("%s BP_UnlockRecipe applied (%d PC recipes; available=%d ours=%d)"),
         PIPELINECOLOR_LOG_PREFIX, Batch.Num(), Available, Ours);
}

void UPipelineColorRootInstanceModule::HandleBuildableBuilt(AFGBuildable* Buildable) {
  if (!IsValid(Buildable)) {
    return;
  }

  if (FPipeSupportTouch::IsPipeSupport(Buildable)) {
    if (AFGBuildablePipeline* Pipe = FPipeSupportTouch::FindTouchedPipe(Buildable)) {
      FPipeSupportTouch::RememberLink(Pipe, Buildable);
      FFluidAppearanceObserver::EnqueueFromWorld(Buildable->GetWorld(), Pipe);
    }
    return;
  }

  if (AFGBuildablePipeline* Pipe = Cast<AFGBuildablePipeline>(Buildable)) {
    FPipeSupportTouch::InvalidateBuildable(Pipe);
  }

  if (!FPipeFluidKeyResolver::IsPipeColorTarget(Buildable)) {
    return;
  }

  FFluidAppearanceObserver::EnqueueFromWorld(Buildable->GetWorld(), Buildable);
}

void UPipelineColorRootInstanceModule::HandlePostLoadMap(UWorld* World) {
  if (!FPCWorldGate::IsGameplayWorld(World)) {
    return;
  }

  FPCFluidAppearanceCatalog::Get().EnsureLoaded();

  World->GetTimerManager().SetTimerForNextTick(
      FTimerDelegate::CreateLambda([WorldWeak = TWeakObjectPtr<UWorld>(World)]() {
        UWorld* WorldPtr = WorldWeak.Get();
        if (!FPCWorldGate::IsGameplayWorld(WorldPtr)) {
          return;
        }

        FPCSwatchPublisher::PublishForWorld(WorldPtr);

        if (UPCWorldSubsystem* Sys = UPCWorldSubsystem::Get(WorldPtr)) {
          Sys->BindSwatchStore(WorldPtr);
        }

        if (WorldPtr->GetNetMode() == NM_Client) {
          return;
        }

        FPipelineColorSmlChatCommands::RegisterWithWorld(WorldPtr);

        if (UPCWorldSubsystem* Sys = UPCWorldSubsystem::Get(WorldPtr)) {
          Sys->OnWorldReady(WorldPtr);
        }
      }));
}

void UPipelineColorRootInstanceModule::DispatchLifecycleEvent(ELifecyclePhase Phase) {
  if (Phase == ELifecyclePhase::POST_INITIALIZATION) {
    FPCPipelineColorModConfig::LoadRuntimeConfig();
#if !WITH_EDITOR
    UE_LOG(LogPipelineColor, Display, TEXT("PipelineColor v1.0.0 — fluid-driven pipe swatches"));
#endif
  }

  if (Phase != ELifecyclePhase::INITIALIZATION) {
    Super::DispatchLifecycleEvent(Phase);
    return;
  }

#if WITH_EDITOR
  UE_LOG(LogPipelineColor, Log, TEXT("%s skip runtime hooks in editor"), PIPELINECOLOR_LOG_PREFIX);
#else
  FFluidAppearanceObserver::RegisterHooks();
  FPCLoadAnnouncement::Register();
  FPCSwatchSlotDispatch::RegisterHooks();
  FPCSwatchPublisher::RegisterForceRecipeHook();

  if (!PostLoadMapHandle.IsValid()) {
    PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddStatic(
        &UPipelineColorRootInstanceModule::HandlePostLoadMap);
  }

  SUBSCRIBE_METHOD_AFTER(AFGBuildableSubsystem::AddBuildable,
                         [](AFGBuildableSubsystem* /*Sub*/, AFGBuildable* Buildable) {
                           UPipelineColorRootInstanceModule::HandleBuildableBuilt(Buildable);
                         });

  SUBSCRIBE_METHOD_VIRTUAL_AFTER(AFGBuildablePipeline::BeginPlay,
                                 GetMutableDefault<AFGBuildablePipeline>(),
                                 [](AFGBuildablePipeline* Pipe) {
                                   UPipelineColorRootInstanceModule::HandleBuildableBuilt(Pipe);
                                 });

  SUBSCRIBE_METHOD_VIRTUAL_AFTER(
      AFGBuildablePipelineAttachment::BeginPlay,
      GetMutableDefault<AFGBuildablePipelineAttachment>(),
      [](AFGBuildablePipelineAttachment* Attachment) {
        UPipelineColorRootInstanceModule::HandleBuildableBuilt(Cast<AFGBuildable>(Attachment));
      });
#endif

  Super::DispatchLifecycleEvent(Phase);
}
