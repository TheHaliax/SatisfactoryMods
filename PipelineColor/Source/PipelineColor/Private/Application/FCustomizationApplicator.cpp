// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Application/FCustomizationApplicator.h"

#include "Buildables/FGBuildable.h"
#include "FGFactoryColoringTypes.h"
#include "PipelineColorLog.h"
#include "UObject/SoftObjectPath.h"

namespace {
TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> LoadCustomSwatch() {
  // No static TSubclassOf cache — not a GC root; soft path TryLoad each call.
  const FSoftClassPath Path(TEXT("/Game/FactoryGame/Buildable/-Shared/Customization/Swatches/"
                                 "SwatchDesc_Custom.SwatchDesc_Custom_C"));
  return Path.TryLoadClass<UFGFactoryCustomizationDescriptor_Swatch>();
}
} // namespace

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

  // Never mutate PaintFinish CDOs (shared or owned). Roughness is finish-class identity
  // via FPCMetallicFinishPool.
  FFactoryCustomizationData Next;
  Next.InlineCombine(Data);
  Next.SwatchDesc = PaintSwatch;
  Next.ColorSlot = INDEX_CUSTOM_COLOR_SLOT;
  Next.OverrideColorData.PrimaryColor = Spec.PrimaryColor;
  Next.OverrideColorData.SecondaryColor = Spec.SecondaryColor;
  Next.OverrideColorData.PaintFinish = Spec.PaintFinish;

  Buildable->SetCustomizationData_Native(Next, /*skipCombine=*/true);

  UE_LOG(LogPipelineColor, Log, TEXT("%s apply %s key=%s primary=(%.2f,%.2f,%.2f)"),
         PIPELINECOLOR_LOG_PREFIX, *GetNameSafe(Buildable), *Spec.CatalogKey.ToString(),
         Spec.PrimaryColor.R, Spec.PrimaryColor.G, Spec.PrimaryColor.B);

  return true;
}
