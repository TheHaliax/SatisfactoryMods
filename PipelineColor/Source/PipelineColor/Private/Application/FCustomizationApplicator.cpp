// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Application/FCustomizationApplicator.h"

#include "Buildables/FGBuildable.h"
#include "FGFactoryColoringTypes.h"
#include "PipelineColorLog.h"
#include "Swatches/UPCFinishDescs.h"
#include "UObject/SoftObjectPath.h"

namespace {
TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> LoadCustomSwatch() {
  static TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Cached;
  if (!Cached) {
    const FSoftClassPath Path(
        TEXT("/Game/FactoryGame/Buildable/-Shared/Customization/Swatches/"
             "SwatchDesc_Custom.SwatchDesc_Custom_C"));
    Cached = Path.TryLoadClass<UFGFactoryCustomizationDescriptor_Swatch>();
  }
  return Cached;
}

struct FScopedFinishRoughnessStamp {
  UFGFactoryCustomizationDescriptor_PaintFinish* CDO = nullptr;
  float SavedRoughness = 0.f;
  float SavedMetallic = 0.f;
  bool bSavedForced = false;
  bool bForceMetallic = false;
  bool bActive = false;

  FScopedFinishRoughnessStamp(TSubclassOf<UFGFactoryCustomizationDescriptor_PaintFinish> Finish,
                              float Roughness, bool bSetMetallicOne) {
    if (!Finish) {
      return;
    }
    CDO = Finish->GetDefaultObject<UFGFactoryCustomizationDescriptor_PaintFinish>();
    if (!CDO) {
      return;
    }
    SavedRoughness = CDO->RoughnessValue;
    SavedMetallic = CDO->MetallicValue;
    bSavedForced = CDO->mHasForcedColor;
    bForceMetallic = bSetMetallicOne;
    CDO->RoughnessValue = Roughness;
    if (bForceMetallic) {
      CDO->MetallicValue = 1.f;
      CDO->mHasForcedColor = false;
    }
    bActive = true;
  }

  ~FScopedFinishRoughnessStamp() {
    if (!bActive || !CDO) {
      return;
    }
    CDO->RoughnessValue = SavedRoughness;
    if (bForceMetallic) {
      CDO->MetallicValue = SavedMetallic;
      CDO->mHasForcedColor = bSavedForced;
    }
  }

  FScopedFinishRoughnessStamp(const FScopedFinishRoughnessStamp&) = delete;
  FScopedFinishRoughnessStamp& operator=(const FScopedFinishRoughnessStamp&) = delete;
};
}  // namespace

bool FCustomizationApplicator::ApplyIfChanged(AFGBuildable* Buildable,
                                              const FPCAppearanceSpec& Spec) {
  if (!IsValid(Buildable) || !Buildable->HasAuthority()) {
    return false;
  }

  TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> PaintSwatch = LoadCustomSwatch();
  if (!PaintSwatch) {
    PaintSwatch = Spec.SwatchDesc;
  }

  FFactoryCustomizationData Data = Buildable->GetCustomizationData_Native();
  const bool bSameColors =
      Data.SwatchDesc == PaintSwatch && Data.ColorSlot == INDEX_CUSTOM_COLOR_SLOT &&
      Data.OverrideColorData.PaintFinish == Spec.PaintFinish &&
      Data.OverrideColorData.PrimaryColor.Equals(Spec.PrimaryColor, 0.001f) &&
      Data.OverrideColorData.SecondaryColor.Equals(Spec.SecondaryColor, 0.001f);

  if (bSameColors && !Spec.bOverrideRoughness) {
    return false;
  }

  const bool bStampRoughness = Spec.bOverrideRoughness && Spec.PaintFinish != nullptr;
  const bool bStampMetallicOne = Spec.PaintFinish == UPCFinish_MetallicColor::StaticClass();

  FFactoryCustomizationData Next;
  Next.InlineCombine(Data);
  Next.SwatchDesc = PaintSwatch;
  Next.ColorSlot = INDEX_CUSTOM_COLOR_SLOT;
  Next.OverrideColorData.PrimaryColor = Spec.PrimaryColor;
  Next.OverrideColorData.SecondaryColor = Spec.SecondaryColor;
  Next.OverrideColorData.PaintFinish = Spec.PaintFinish;

  if (bStampRoughness) {
    FScopedFinishRoughnessStamp Stamp(Spec.PaintFinish, Spec.RoughnessValue, bStampMetallicOne);
    Buildable->SetCustomizationData_Native(Next, /*skipCombine=*/true);
  } else {
    Buildable->SetCustomizationData_Native(Next, /*skipCombine=*/true);
  }

  UE_LOG(LogPipelineColor, Log, TEXT("%s apply %s key=%s primary=(%.2f,%.2f,%.2f)"),
         PIPELINECOLOR_LOG_PREFIX, *GetNameSafe(Buildable), *Spec.CatalogKey.ToString(),
         Spec.PrimaryColor.R, Spec.PrimaryColor.G, Spec.PrimaryColor.B);

  return true;
}
