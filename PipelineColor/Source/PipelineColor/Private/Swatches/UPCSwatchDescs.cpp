// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Swatches/UPCSwatchDescs.h"

#define LOCTEXT_NAMESPACE "PipelineColor"

namespace {
void InitSwatch(UPCSwatchDescBase* Self, const FText& Name, FName Key) {
  if (!Self) {
    return;
  }

  Self->mUseDisplayNameAndDescription = true;
  Self->mDisplayName = Name;
  Self->mDescription = NSLOCTEXT("PipelineColor", "SwatchDesc", "PipelineColor fluid swatch");
  Self->ID = INDEX_CUSTOM_COLOR_SLOT;
  Self->CatalogKey = Key;
  Self->mValidBuildables.Reset();
}
}  // namespace

UPCSwatchDescBase::UPCSwatchDescBase() {
  mUseDisplayNameAndDescription = true;
  ID = INDEX_CUSTOM_COLOR_SLOT;
}

FName UPCSwatchDescBase::GetCatalogKey(
    TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Swatch) {
  if (!Swatch) {
    return NAME_None;
  }
  if (const UPCSwatchDescBase* CDO = Cast<UPCSwatchDescBase>(Swatch->GetDefaultObject())) {
    return CDO->CatalogKey;
  }
  return NAME_None;
}

UPCSwatchDesc_Neutral::UPCSwatchDesc_Neutral() {
  InitSwatch(this, NSLOCTEXT("PipelineColor", "SwatchNeutral", "PC Neutral"),
             FName(TEXT("Neutral")));
}

UPCSwatchDesc_Water::UPCSwatchDesc_Water() {
  InitSwatch(this, NSLOCTEXT("PipelineColor", "SwatchWater", "PC Water"), FName(TEXT("Water")));
}

UPCSwatchDesc_CrudeOil::UPCSwatchDesc_CrudeOil() {
  InitSwatch(this, NSLOCTEXT("PipelineColor", "SwatchCrudeOil", "PC Crude Oil"),
             FName(TEXT("CrudeOil")));
}

UPCSwatchDesc_HeavyOilResidue::UPCSwatchDesc_HeavyOilResidue() {
  InitSwatch(this, NSLOCTEXT("PipelineColor", "SwatchHeavyOilResidue", "PC Heavy Oil Residue"),
             FName(TEXT("HeavyOilResidue")));
}

UPCSwatchDesc_Fuel::UPCSwatchDesc_Fuel() {
  InitSwatch(this, NSLOCTEXT("PipelineColor", "SwatchFuel", "PC Fuel"), FName(TEXT("Fuel")));
}

UPCSwatchDesc_Turbofuel::UPCSwatchDesc_Turbofuel() {
  InitSwatch(this, NSLOCTEXT("PipelineColor", "SwatchTurbofuel", "PC Turbofuel"),
             FName(TEXT("Turbofuel")));
}

UPCSwatchDesc_LiquidBiofuel::UPCSwatchDesc_LiquidBiofuel() {
  InitSwatch(this, NSLOCTEXT("PipelineColor", "SwatchLiquidBiofuel", "PC Liquid Biofuel"),
             FName(TEXT("LiquidBiofuel")));
}

UPCSwatchDesc_AluminaSolution::UPCSwatchDesc_AluminaSolution() {
  InitSwatch(this, NSLOCTEXT("PipelineColor", "SwatchAluminaSolution", "PC Alumina Solution"),
             FName(TEXT("AluminaSolution")));
}

UPCSwatchDesc_SulfuricAcid::UPCSwatchDesc_SulfuricAcid() {
  InitSwatch(this, NSLOCTEXT("PipelineColor", "SwatchSulfuricAcid", "PC Sulfuric Acid"),
             FName(TEXT("SulfuricAcid")));
}

UPCSwatchDesc_DissolvedSilica::UPCSwatchDesc_DissolvedSilica() {
  InitSwatch(this, NSLOCTEXT("PipelineColor", "SwatchDissolvedSilica", "PC Dissolved Silica"),
             FName(TEXT("DissolvedSilica")));
}

UPCSwatchDesc_NitricAcid::UPCSwatchDesc_NitricAcid() {
  InitSwatch(this, NSLOCTEXT("PipelineColor", "SwatchNitricAcid", "PC Nitric Acid"),
             FName(TEXT("NitricAcid")));
}

UPCSwatchDesc_DarkMatterResidue::UPCSwatchDesc_DarkMatterResidue() {
  InitSwatch(this, NSLOCTEXT("PipelineColor", "SwatchDarkMatterResidue", "PC Dark Matter Residue"),
             FName(TEXT("DarkMatterResidue")));
}

UPCSwatchDesc_ExcitedPhotonicMatter::UPCSwatchDesc_ExcitedPhotonicMatter() {
  InitSwatch(
      this, NSLOCTEXT("PipelineColor", "SwatchExcitedPhotonicMatter", "PC Excited Photonic Matter"),
      FName(TEXT("ExcitedPhotonicMatter")));
}

UPCSwatchDesc_IonizedFuel::UPCSwatchDesc_IonizedFuel() {
  InitSwatch(this, NSLOCTEXT("PipelineColor", "SwatchIonizedFuel", "PC Ionized Fuel"),
             FName(TEXT("IonizedFuel")));
}

UPCSwatchDesc_RocketFuel::UPCSwatchDesc_RocketFuel() {
  InitSwatch(this, NSLOCTEXT("PipelineColor", "SwatchRocketFuel", "PC Rocket Fuel"),
             FName(TEXT("RocketFuel")));
}

UPCSwatchDesc_NitrogenGas::UPCSwatchDesc_NitrogenGas() {
  InitSwatch(this, NSLOCTEXT("PipelineColor", "SwatchNitrogenGas", "PC Nitrogen Gas"),
             FName(TEXT("NitrogenGas")));
}

UPCSwatchDesc_Fallback::UPCSwatchDesc_Fallback() {
  InitSwatch(this, NSLOCTEXT("PipelineColor", "SwatchFallback", "PC Fallback"),
             FName(TEXT("Fallback")));
}

#undef LOCTEXT_NAMESPACE
