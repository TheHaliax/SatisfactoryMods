// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Swatches/UPCSwatchDescs_Sfp.h"

#define LOCTEXT_NAMESPACE "PipelineColor"

namespace {
void InitSwatchSfp(UPCSwatchDescBase* Self, const FText& Name, FName Key) {
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

UPCSwatchDesc_Air::UPCSwatchDesc_Air() {
  InitSwatchSfp(this, NSLOCTEXT("PipelineColor", "SwatchAir", "PC Air"), FName(TEXT("Air")));
}

UPCSwatchDesc_CausticSoda::UPCSwatchDesc_CausticSoda() {
  InitSwatchSfp(this, NSLOCTEXT("PipelineColor", "SwatchCausticSoda", "PC Caustic Soda"),
                FName(TEXT("CausticSoda")));
}

UPCSwatchDesc_CoolingWater::UPCSwatchDesc_CoolingWater() {
  InitSwatchSfp(this, NSLOCTEXT("PipelineColor", "SwatchCoolingWater", "PC Cooling Water"),
                FName(TEXT("CoolingWater")));
}

UPCSwatchDesc_DieselOil::UPCSwatchDesc_DieselOil() {
  InitSwatchSfp(this, NSLOCTEXT("PipelineColor", "SwatchDieselOil", "PC Diesel Fuel"),
                FName(TEXT("DieselOil")));
}

UPCSwatchDesc_Ethylene::UPCSwatchDesc_Ethylene() {
  InitSwatchSfp(this, NSLOCTEXT("PipelineColor", "SwatchEthylene", "PC Ethylene Gas"),
                FName(TEXT("Ethylene")));
}

UPCSwatchDesc_FlueGas::UPCSwatchDesc_FlueGas() {
  InitSwatchSfp(this, NSLOCTEXT("PipelineColor", "SwatchFlueGas", "PC Flue Gas"),
                FName(TEXT("FlueGas")));
}

UPCSwatchDesc_Fluid_RocketFuel::UPCSwatchDesc_Fluid_RocketFuel() {
  InitSwatchSfp(this, NSLOCTEXT("PipelineColor", "SwatchFluid_RocketFuel", "PC Rocket Fuel"),
                FName(TEXT("Fluid_RocketFuel")));
}

UPCSwatchDesc_Gas_Chlor::UPCSwatchDesc_Gas_Chlor() {
  InitSwatchSfp(this, NSLOCTEXT("PipelineColor", "SwatchGas_Chlor", "PC Chlorine"),
                FName(TEXT("Gas_Chlor")));
}

UPCSwatchDesc_Gas_Hydrogen::UPCSwatchDesc_Gas_Hydrogen() {
  InitSwatchSfp(this, NSLOCTEXT("PipelineColor", "SwatchGas_Hydrogen", "PC Hydrogen"),
                FName(TEXT("Gas_Hydrogen")));
}

UPCSwatchDesc_Gas_Oxyhydrogen::UPCSwatchDesc_Gas_Oxyhydrogen() {
  InitSwatchSfp(this, NSLOCTEXT("PipelineColor", "SwatchGas_Oxyhydrogen", "PC Oxyhydrogen"),
                FName(TEXT("Gas_Oxyhydrogen")));
}

UPCSwatchDesc_Gas_Steam::UPCSwatchDesc_Gas_Steam() {
  InitSwatchSfp(this, NSLOCTEXT("PipelineColor", "SwatchGas_Steam", "PC Steam"),
                FName(TEXT("Gas_Steam")));
}

UPCSwatchDesc_Gas_Steam_Hot::UPCSwatchDesc_Gas_Steam_Hot() {
  InitSwatchSfp(this, NSLOCTEXT("PipelineColor", "SwatchGas_Steam_Hot", "PC Hot Steam"),
                FName(TEXT("Gas_Steam_Hot")));
}

UPCSwatchDesc_Gas_Titantetrachlorid::UPCSwatchDesc_Gas_Titantetrachlorid() {
  InitSwatchSfp(this, NSLOCTEXT("PipelineColor", "SwatchGas_Titantetrachlorid", "PC TiCl Gas"),
                FName(TEXT("Gas_Titantetrachlorid")));
}

UPCSwatchDesc_KLIB_NaturalGas::UPCSwatchDesc_KLIB_NaturalGas() {
  InitSwatchSfp(this, NSLOCTEXT("PipelineColor", "SwatchKLIB_NaturalGas", "PC Natural Gas"),
                FName(TEXT("KLIB_NaturalGas")));
}

UPCSwatchDesc_LiquidAluminum::UPCSwatchDesc_LiquidAluminum() {
  InitSwatchSfp(this, NSLOCTEXT("PipelineColor", "SwatchLiquidAluminum", "PC Molten Aluminum"),
                FName(TEXT("LiquidAluminum")));
}

UPCSwatchDesc_LiquidBrass::UPCSwatchDesc_LiquidBrass() {
  InitSwatchSfp(this, NSLOCTEXT("PipelineColor", "SwatchLiquidBrass", "PC Molten Brass"),
                FName(TEXT("LiquidBrass")));
}

UPCSwatchDesc_LiquidBronze::UPCSwatchDesc_LiquidBronze() {
  InitSwatchSfp(this, NSLOCTEXT("PipelineColor", "SwatchLiquidBronze", "PC Molten Bronze"),
                FName(TEXT("LiquidBronze")));
}

UPCSwatchDesc_LiquidCobalt::UPCSwatchDesc_LiquidCobalt() {
  InitSwatchSfp(this, NSLOCTEXT("PipelineColor", "SwatchLiquidCobalt", "PC Molten Cobalt"),
                FName(TEXT("LiquidCobalt")));
}

UPCSwatchDesc_LiquidCopper::UPCSwatchDesc_LiquidCopper() {
  InitSwatchSfp(this, NSLOCTEXT("PipelineColor", "SwatchLiquidCopper", "PC Molten Copper"),
                FName(TEXT("LiquidCopper")));
}

UPCSwatchDesc_LiquidGold::UPCSwatchDesc_LiquidGold() {
  InitSwatchSfp(this, NSLOCTEXT("PipelineColor", "SwatchLiquidGold", "PC Molten Caterium"),
                FName(TEXT("LiquidGold")));
}

UPCSwatchDesc_LiquidHydrochloricAcid::UPCSwatchDesc_LiquidHydrochloricAcid() {
  InitSwatchSfp(this,
                NSLOCTEXT("PipelineColor", "SwatchLiquidHydrochloricAcid", "PC Muriatic Acid"),
                FName(TEXT("LiquidHydrochloricAcid")));
}

UPCSwatchDesc_LiquidInvar::UPCSwatchDesc_LiquidInvar() {
  InitSwatchSfp(this, NSLOCTEXT("PipelineColor", "SwatchLiquidInvar", "PC Molten Invar"),
                FName(TEXT("LiquidInvar")));
}

UPCSwatchDesc_LiquidIron::UPCSwatchDesc_LiquidIron() {
  InitSwatchSfp(this, NSLOCTEXT("PipelineColor", "SwatchLiquidIron", "PC Molten Iron"),
                FName(TEXT("LiquidIron")));
}

UPCSwatchDesc_LiquidLead::UPCSwatchDesc_LiquidLead() {
  InitSwatchSfp(this, NSLOCTEXT("PipelineColor", "SwatchLiquidLead", "PC Molten Lead"),
                FName(TEXT("LiquidLead")));
}

UPCSwatchDesc_LiquidMagnesium::UPCSwatchDesc_LiquidMagnesium() {
  InitSwatchSfp(this, NSLOCTEXT("PipelineColor", "SwatchLiquidMagnesium", "PC Molten Magnesium"),
                FName(TEXT("LiquidMagnesium")));
}

UPCSwatchDesc_LiquidNickle::UPCSwatchDesc_LiquidNickle() {
  InitSwatchSfp(this, NSLOCTEXT("PipelineColor", "SwatchLiquidNickle", "PC Molten Nickel"),
                FName(TEXT("LiquidNickle")));
}

UPCSwatchDesc_LiquidRawSludge::UPCSwatchDesc_LiquidRawSludge() {
  InitSwatchSfp(this, NSLOCTEXT("PipelineColor", "SwatchLiquidRawSludge", "PC Raw Sludge"),
                FName(TEXT("LiquidRawSludge")));
}

UPCSwatchDesc_LiquidSlag::UPCSwatchDesc_LiquidSlag() {
  InitSwatchSfp(this, NSLOCTEXT("PipelineColor", "SwatchLiquidSlag", "PC Tailings Slurry"),
                FName(TEXT("LiquidSlag")));
}

UPCSwatchDesc_LiquidSolder::UPCSwatchDesc_LiquidSolder() {
  InitSwatchSfp(this, NSLOCTEXT("PipelineColor", "SwatchLiquidSolder", "PC Molten Solder"),
                FName(TEXT("LiquidSolder")));
}

UPCSwatchDesc_LiquidSteel::UPCSwatchDesc_LiquidSteel() {
  InitSwatchSfp(this, NSLOCTEXT("PipelineColor", "SwatchLiquidSteel", "PC Molten Steel"),
                FName(TEXT("LiquidSteel")));
}

UPCSwatchDesc_LiquidTin::UPCSwatchDesc_LiquidTin() {
  InitSwatchSfp(this, NSLOCTEXT("PipelineColor", "SwatchLiquidTin", "PC Molten Tin"),
                FName(TEXT("LiquidTin")));
}

UPCSwatchDesc_LiquidZinc::UPCSwatchDesc_LiquidZinc() {
  InitSwatchSfp(this, NSLOCTEXT("PipelineColor", "SwatchLiquidZinc", "PC Molten Zinc"),
                FName(TEXT("LiquidZinc")));
}

UPCSwatchDesc_Naphtha::UPCSwatchDesc_Naphtha() {
  InitSwatchSfp(this, NSLOCTEXT("PipelineColor", "SwatchNaphtha", "PC Petroleum Benzine"),
                FName(TEXT("Naphtha")));
}

UPCSwatchDesc_Neutronium::UPCSwatchDesc_Neutronium() {
  InitSwatchSfp(this, NSLOCTEXT("PipelineColor", "SwatchNeutronium", "PC Condensed Neutrium Gas"),
                FName(TEXT("Neutronium")));
}

UPCSwatchDesc_Paint::UPCSwatchDesc_Paint() {
  InitSwatchSfp(this, NSLOCTEXT("PipelineColor", "SwatchPaint", "PC Paint"), FName(TEXT("Paint")));
}

UPCSwatchDesc_Paraffin::UPCSwatchDesc_Paraffin() {
  InitSwatchSfp(this, NSLOCTEXT("PipelineColor", "SwatchParaffin", "PC Paraffin"),
                FName(TEXT("Paraffin")));
}

UPCSwatchDesc_PurifiedCrudeOil::UPCSwatchDesc_PurifiedCrudeOil() {
  InitSwatchSfp(this, NSLOCTEXT("PipelineColor", "SwatchPurifiedCrudeOil", "PC Purified Crude Oil"),
                FName(TEXT("PurifiedCrudeOil")));
}

UPCSwatchDesc_SlugSlime_High::UPCSwatchDesc_SlugSlime_High() {
  InitSwatchSfp(this, NSLOCTEXT("PipelineColor", "SwatchSlugSlime_High", "PC Energized Slug Slime"),
                FName(TEXT("SlugSlime_High")));
}

UPCSwatchDesc_SlugSlime_Low::UPCSwatchDesc_SlugSlime_Low() {
  InitSwatchSfp(this, NSLOCTEXT("PipelineColor", "SwatchSlugSlime_Low", "PC Slug Slime"),
                FName(TEXT("SlugSlime_Low")));
}

UPCSwatchDesc_SlugSlime_Mid::UPCSwatchDesc_SlugSlime_Mid() {
  InitSwatchSfp(this, NSLOCTEXT("PipelineColor", "SwatchSlugSlime_Mid", "PC Spent Slug Slime"),
                FName(TEXT("SlugSlime_Mid")));
}

UPCSwatchDesc_SlugSlime_Mineral::UPCSwatchDesc_SlugSlime_Mineral() {
  InitSwatchSfp(this, NSLOCTEXT("PipelineColor", "SwatchSlugSlime_Mineral", "PC Universal Paint"),
                FName(TEXT("SlugSlime_Mineral")));
}

UPCSwatchDesc_Sulfurtrixid::UPCSwatchDesc_Sulfurtrixid() {
  InitSwatchSfp(this, NSLOCTEXT("PipelineColor", "SwatchSulfurtrixid", "PC Sour Gas"),
                FName(TEXT("Sulfurtrixid")));
}

UPCSwatchDesc_TiHy::UPCSwatchDesc_TiHy() {
  InitSwatchSfp(this, NSLOCTEXT("PipelineColor", "SwatchTiHy", "PC TiHy Solution"),
                FName(TEXT("TiHy")));
}

UPCSwatchDesc_ToxicAir::UPCSwatchDesc_ToxicAir() {
  InitSwatchSfp(this, NSLOCTEXT("PipelineColor", "SwatchToxicAir", "PC Toxic Air"),
                FName(TEXT("ToxicAir")));
}

UPCSwatchDesc_UranChloride::UPCSwatchDesc_UranChloride() {
  InitSwatchSfp(this, NSLOCTEXT("PipelineColor", "SwatchUranChloride", "PC Uranium Chloride"),
                FName(TEXT("UranChloride")));
}

UPCSwatchDesc_Wastewater::UPCSwatchDesc_Wastewater() {
  InitSwatchSfp(this, NSLOCTEXT("PipelineColor", "SwatchWastewater", "PC Wastewater"),
                FName(TEXT("Wastewater")));
}

#undef LOCTEXT_NAMESPACE
