// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Swatches/UPCSwatchDescs_Rp.h"

#define LOCTEXT_NAMESPACE "PipelineColor"

namespace {
void InitSwatchRp(UPCSwatchDescBase* Self, const FText& Name, FName Key) {
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
} // namespace

UPCSwatchDesc_RP_CloudyCoolant::UPCSwatchDesc_RP_CloudyCoolant() {
  InitSwatchRp(this, NSLOCTEXT("PipelineColor", "SwatchRP_CloudyCoolant", "PC Cloudy Coolant"),
               FName(TEXT("RP_CloudyCoolant")));
}

UPCSwatchDesc_RP_Co2::UPCSwatchDesc_RP_Co2() {
  InitSwatchRp(this, NSLOCTEXT("PipelineColor", "SwatchRP_Co2", "PC CO2"), FName(TEXT("RP_Co2")));
}

UPCSwatchDesc_RP_CompressedSteam::UPCSwatchDesc_RP_CompressedSteam() {
  InitSwatchRp(this, NSLOCTEXT("PipelineColor", "SwatchRP_CompressedSteam", "PC Compressed Steam"),
               FName(TEXT("RP_CompressedSteam")));
}

UPCSwatchDesc_RP_ConcentratedDeanium::UPCSwatchDesc_RP_ConcentratedDeanium() {
  InitSwatchRp(
      this, NSLOCTEXT("PipelineColor", "SwatchRP_ConcentratedDeanium", "PC Concentrated Deanium"),
      FName(TEXT("RP_ConcentratedDeanium")));
}

UPCSwatchDesc_RP_Deanium::UPCSwatchDesc_RP_Deanium() {
  InitSwatchRp(this, NSLOCTEXT("PipelineColor", "SwatchRP_Deanium", "PC Deanium"),
               FName(TEXT("RP_Deanium")));
}

UPCSwatchDesc_RP_Deanorium::UPCSwatchDesc_RP_Deanorium() {
  InitSwatchRp(this, NSLOCTEXT("PipelineColor", "SwatchRP_Deanorium", "PC Deanorium"),
               FName(TEXT("RP_Deanorium")));
}

UPCSwatchDesc_RP_HighSteam::UPCSwatchDesc_RP_HighSteam() {
  InitSwatchRp(this, NSLOCTEXT("PipelineColor", "SwatchRP_HighSteam", "PC High Pressure Steam"),
               FName(TEXT("RP_HighSteam")));
}

UPCSwatchDesc_RP_HotMoltenSalt::UPCSwatchDesc_RP_HotMoltenSalt() {
  InitSwatchRp(this, NSLOCTEXT("PipelineColor", "SwatchRP_HotMoltenSalt", "PC Hot Molten Salt"),
               FName(TEXT("RP_HotMoltenSalt")));
}

UPCSwatchDesc_RP_LiquidDeuterium::UPCSwatchDesc_RP_LiquidDeuterium() {
  InitSwatchRp(this, NSLOCTEXT("PipelineColor", "SwatchRP_LiquidDeuterium", "PC Liquid Deuterium"),
               FName(TEXT("RP_LiquidDeuterium")));
}

UPCSwatchDesc_RP_LiquidDeuteriumTritiumBlend::UPCSwatchDesc_RP_LiquidDeuteriumTritiumBlend() {
  InitSwatchRp(this,
               NSLOCTEXT("PipelineColor", "SwatchRP_LiquidDeuteriumTritiumBlend", "PC D-T Blend"),
               FName(TEXT("RP_LiquidDeuteriumTritiumBlend")));
}

UPCSwatchDesc_RP_LiquidTritium::UPCSwatchDesc_RP_LiquidTritium() {
  InitSwatchRp(this, NSLOCTEXT("PipelineColor", "SwatchRP_LiquidTritium", "PC Liquid Tritium"),
               FName(TEXT("RP_LiquidTritium")));
}

UPCSwatchDesc_RP_LowSteam::UPCSwatchDesc_RP_LowSteam() {
  InitSwatchRp(this, NSLOCTEXT("PipelineColor", "SwatchRP_LowSteam", "PC Low Pressure Steam"),
               FName(TEXT("RP_LowSteam")));
}

UPCSwatchDesc_RP_MethaneGas::UPCSwatchDesc_RP_MethaneGas() {
  InitSwatchRp(this, NSLOCTEXT("PipelineColor", "SwatchRP_MethaneGas", "PC Methane Gas"),
               FName(TEXT("RP_MethaneGas")));
}

UPCSwatchDesc_RP_MoltenSalt::UPCSwatchDesc_RP_MoltenSalt() {
  InitSwatchRp(this, NSLOCTEXT("PipelineColor", "SwatchRP_MoltenSalt", "PC Molten Salt"),
               FName(TEXT("RP_MoltenSalt")));
}

UPCSwatchDesc_RP_ReactorCoolant::UPCSwatchDesc_RP_ReactorCoolant() {
  InitSwatchRp(this, NSLOCTEXT("PipelineColor", "SwatchRP_ReactorCoolant", "PC Reactor Coolant"),
               FName(TEXT("RP_ReactorCoolant")));
}

UPCSwatchDesc_RP_ToxicGas::UPCSwatchDesc_RP_ToxicGas() {
  InitSwatchRp(this, NSLOCTEXT("PipelineColor", "SwatchRP_ToxicGas", "PC Toxic Gas"),
               FName(TEXT("RP_ToxicGas")));
}

#undef LOCTEXT_NAMESPACE
