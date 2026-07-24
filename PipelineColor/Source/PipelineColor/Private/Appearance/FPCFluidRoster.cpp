// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Appearance/FPCFluidRoster.h"

#include "Resources/FGItemDescriptor.h"
#include "Swatches/UPCSwatchDescs.h"
#include "Swatches/UPCSwatchDescs_Rp.h"
#include "Swatches/UPCSwatchDescs_Sfp.h"
#include "Swatches/UPCSwatchRecipes.h"
#include "Swatches/UPCSwatchRecipes_Rp.h"
#include "Swatches/UPCSwatchRecipes_Sfp.h"
#include "UObject/SoftObjectPath.h"

namespace FPCFluidRoster {
const TArray<FPCFluidRosterRow>& FluidRows() {
  static TArray<FPCFluidRosterRow> Rows;
  static bool bReady = false;
  if (bReady) {
    return Rows;
  }
  bReady = true;

  auto Add = [&](const TCHAR* Path, const TCHAR* Stem, uint8 R, uint8 G, uint8 B,
                 EPCPaintFinishKind Finish,
                 TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Swatch,
                 TSubclassOf<UFGCustomizationRecipe> Recipe,
                 EPCFluidRosterSection Section = EPCFluidRosterSection::Default) {
    FPCFluidRosterRow Row;
    Row.SoftPath = Path;
    Row.Stem = FName(Stem);
    Row.PrimaryR = R;
    Row.PrimaryG = G;
    Row.PrimaryB = B;
    Row.Finish = Finish;
    Row.SwatchClass = Swatch;
    Row.RecipeClass = Recipe;
    Row.Section = Section;
    Rows.Add(Row);
  };

  Add(TEXT("/Game/FactoryGame/Resource/RawResources/Water/Desc_Water.Desc_Water_C"), TEXT("Water"),
      0x7A, 0xB0, 0xD4, EPCPaintFinishKind::Default, UPCSwatchDesc_Water::StaticClass(),
      UPCSwatchRecipe_Water::StaticClass());
  Add(TEXT("/Game/FactoryGame/Resource/RawResources/CrudeOil/Desc_LiquidOil.Desc_LiquidOil_C"),
      TEXT("CrudeOil"), 0x19, 0x00, 0x19, EPCPaintFinishKind::Default,
      UPCSwatchDesc_CrudeOil::StaticClass(), UPCSwatchRecipe_CrudeOil::StaticClass());
  Add(TEXT("/Game/FactoryGame/Resource/Parts/HeavyOilResidue/"
           "Desc_HeavyOilResidue.Desc_HeavyOilResidue_C"),
      TEXT("HeavyOilResidue"), 0x6D, 0x2D, 0x78, EPCPaintFinishKind::Default,
      UPCSwatchDesc_HeavyOilResidue::StaticClass(), UPCSwatchRecipe_HeavyOilResidue::StaticClass());
  Add(TEXT("/Game/FactoryGame/Resource/Parts/Fuel/Desc_LiquidFuel.Desc_LiquidFuel_C"), TEXT("Fuel"),
      0xEB, 0x7D, 0x15, EPCPaintFinishKind::Default, UPCSwatchDesc_Fuel::StaticClass(),
      UPCSwatchRecipe_Fuel::StaticClass());
  Add(TEXT("/Game/FactoryGame/Resource/Parts/Turbofuel/Desc_TurboFuel.Desc_TurboFuel_C"),
      TEXT("Turbofuel"), 0xD4, 0x29, 0x2E, EPCPaintFinishKind::Default,
      UPCSwatchDesc_Turbofuel::StaticClass(), UPCSwatchRecipe_Turbofuel::StaticClass());
  Add(TEXT("/Game/FactoryGame/Resource/Parts/BioFuel/"
           "Desc_LiquidBiofuel.Desc_LiquidBiofuel_C"),
      TEXT("LiquidBiofuel"), 0x3B, 0x53, 0x2C, EPCPaintFinishKind::Default,
      UPCSwatchDesc_LiquidBiofuel::StaticClass(), UPCSwatchRecipe_LiquidBiofuel::StaticClass());
  Add(TEXT("/Game/FactoryGame/Resource/Parts/Alumina/"
           "Desc_AluminaSolution.Desc_AluminaSolution_C"),
      TEXT("AluminaSolution"), 0xC1, 0xC1, 0xC1, EPCPaintFinishKind::Default,
      UPCSwatchDesc_AluminaSolution::StaticClass(), UPCSwatchRecipe_AluminaSolution::StaticClass());
  Add(TEXT("/Game/FactoryGame/Resource/Parts/SulfuricAcid/"
           "Desc_SulfuricAcid.Desc_SulfuricAcid_C"),
      TEXT("SulfuricAcid"), 0xFF, 0xFF, 0x00, EPCPaintFinishKind::Default,
      UPCSwatchDesc_SulfuricAcid::StaticClass(), UPCSwatchRecipe_SulfuricAcid::StaticClass());
  Add(TEXT("/Game/FactoryGame/Resource/Parts/DissolvedSilica/"
           "Desc_DissolvedSilica.Desc_DissolvedSilica_C"),
      TEXT("DissolvedSilica"), 0xE2, 0xBE, 0xEE, EPCPaintFinishKind::Default,
      UPCSwatchDesc_DissolvedSilica::StaticClass(), UPCSwatchRecipe_DissolvedSilica::StaticClass());
  Add(TEXT("/Game/FactoryGame/Resource/Parts/NitricAcid/Desc_NitricAcid.Desc_NitricAcid_C"),
      TEXT("NitricAcid"), 0xD9, 0xD9, 0xA2, EPCPaintFinishKind::Default,
      UPCSwatchDesc_NitricAcid::StaticClass(), UPCSwatchRecipe_NitricAcid::StaticClass());
  Add(TEXT("/Game/FactoryGame/Resource/Parts/DarkEnergy/Desc_DarkEnergy.Desc_DarkEnergy_C"),
      TEXT("DarkMatterResidue"), 0xFD, 0xAF, 0xF9, EPCPaintFinishKind::MetallicColor,
      UPCSwatchDesc_DarkMatterResidue::StaticClass(),
      UPCSwatchRecipe_DarkMatterResidue::StaticClass());
  Add(TEXT("/Game/FactoryGame/Resource/Parts/QuantumEnergy/"
           "Desc_QuantumEnergy.Desc_QuantumEnergy_C"),
      TEXT("ExcitedPhotonicMatter"), 0x76, 0xF5, 0xE8, EPCPaintFinishKind::MetallicColor,
      UPCSwatchDesc_ExcitedPhotonicMatter::StaticClass(),
      UPCSwatchRecipe_ExcitedPhotonicMatter::StaticClass());
  Add(TEXT("/Game/FactoryGame/Resource/Parts/IonizedFuel/"
           "Desc_IonizedFuel.Desc_IonizedFuel_C"),
      TEXT("IonizedFuel"), 0xD5, 0x5F, 0x1A, EPCPaintFinishKind::MetallicColor,
      UPCSwatchDesc_IonizedFuel::StaticClass(), UPCSwatchRecipe_IonizedFuel::StaticClass());
  Add(TEXT("/Game/FactoryGame/Resource/Parts/RocketFuel/Desc_RocketFuel.Desc_RocketFuel_C"),
      TEXT("RocketFuel"), 0xBD, 0x25, 0x1A, EPCPaintFinishKind::MetallicColor,
      UPCSwatchDesc_RocketFuel::StaticClass(), UPCSwatchRecipe_RocketFuel::StaticClass());
  Add(TEXT("/Game/FactoryGame/Resource/RawResources/NitrogenGas/"
           "Desc_NitrogenGas.Desc_NitrogenGas_C"),
      TEXT("NitrogenGas"), 0x59, 0x59, 0x59, EPCPaintFinishKind::MetallicColor,
      UPCSwatchDesc_NitrogenGas::StaticClass(), UPCSwatchRecipe_NitrogenGas::StaticClass());

  Add(TEXT("/RefinedPower/Resource/RawResources/CloudyCoolant/"
           "Desc_RP_CloudyCoolant.Desc_RP_CloudyCoolant_C"),
      TEXT("RP_CloudyCoolant"), 0x91, 0xAA, 0x99, EPCPaintFinishKind::Default,
      UPCSwatchDesc_RP_CloudyCoolant::StaticClass(),
      UPCSwatchRecipe_RP_CloudyCoolant::StaticClass(), EPCFluidRosterSection::Rp);
  Add(TEXT("/RefinedPower/Resource/RawResources/Co2/Desc_RP_Co2.Desc_RP_Co2_C"), TEXT("RP_Co2"),
      0x50, 0x38, 0xA9, EPCPaintFinishKind::MetallicColor, UPCSwatchDesc_RP_Co2::StaticClass(),
      UPCSwatchRecipe_RP_Co2::StaticClass(), EPCFluidRosterSection::Rp);
  Add(TEXT("/RefinedPower/Resource/RawResources/ExtremeHighSteam/"
           "Desc_RP_CompressedSteam.Desc_RP_CompressedSteam_C"),
      TEXT("RP_CompressedSteam"), 0xB3, 0xB3, 0xB3, EPCPaintFinishKind::MetallicColor,
      UPCSwatchDesc_RP_CompressedSteam::StaticClass(),
      UPCSwatchRecipe_RP_CompressedSteam::StaticClass(), EPCFluidRosterSection::Rp);
  Add(TEXT("/RefinedPower/Resource/Parts/ConcentratedDeanium/"
           "Desc_RP_ConcentratedDeanium.Desc_RP_ConcentratedDeanium_C"),
      TEXT("RP_ConcentratedDeanium"), 0xA7, 0x00, 0x30, EPCPaintFinishKind::Default,
      UPCSwatchDesc_RP_ConcentratedDeanium::StaticClass(),
      UPCSwatchRecipe_RP_ConcentratedDeanium::StaticClass(), EPCFluidRosterSection::Rp);
  Add(TEXT("/RefinedPower/Resource/RawResources/Deanium/Desc_RP_Deanium.Desc_RP_Deanium_C"),
      TEXT("RP_Deanium"), 0xCD, 0x00, 0x00, EPCPaintFinishKind::Default,
      UPCSwatchDesc_RP_Deanium::StaticClass(), UPCSwatchRecipe_RP_Deanium::StaticClass(),
      EPCFluidRosterSection::Rp);
  Add(TEXT("/RefinedPower/Resource/Parts/Deanorium/Desc_RP_Deanorium.Desc_RP_Deanorium_C"),
      TEXT("RP_Deanorium"), 0x61, 0x00, 0x2C, EPCPaintFinishKind::Default,
      UPCSwatchDesc_RP_Deanorium::StaticClass(), UPCSwatchRecipe_RP_Deanorium::StaticClass(),
      EPCFluidRosterSection::Rp);
  Add(TEXT("/RefinedPower/Resource/RawResources/HighSteam/Desc_RP_HighSteam.Desc_RP_HighSteam_C"),
      TEXT("RP_HighSteam"), 0x83, 0x83, 0x83, EPCPaintFinishKind::MetallicColor,
      UPCSwatchDesc_RP_HighSteam::StaticClass(), UPCSwatchRecipe_RP_HighSteam::StaticClass(),
      EPCFluidRosterSection::Rp);
  Add(TEXT("/RefinedPower/Resource/RawResources/MoltenSalt/"
           "Desc_RP_HotMoltenSalt.Desc_RP_HotMoltenSalt_C"),
      TEXT("RP_HotMoltenSalt"), 0xF9, 0x66, 0xFF, EPCPaintFinishKind::MetallicColor,
      UPCSwatchDesc_RP_HotMoltenSalt::StaticClass(),
      UPCSwatchRecipe_RP_HotMoltenSalt::StaticClass(), EPCFluidRosterSection::Rp);
  Add(TEXT("/RefinedPower/Resource/RawResources/LiquidDeuterium/"
           "Desc_RP_LiquidDeuterium.Desc_RP_LiquidDeuterium_C"),
      TEXT("RP_LiquidDeuterium"), 0x00, 0xFF, 0xFC, EPCPaintFinishKind::Default,
      UPCSwatchDesc_RP_LiquidDeuterium::StaticClass(),
      UPCSwatchRecipe_RP_LiquidDeuterium::StaticClass(), EPCFluidRosterSection::Rp);
  Add(TEXT("/RefinedPower/Resource/RawResources/DeuteriumTritiumBlend/"
           "Desc_RP_LiquidDeuteriumTritiumBlend.Desc_RP_LiquidDeuteriumTritiumBlend_C"),
      TEXT("RP_LiquidDeuteriumTritiumBlend"), 0x9D, 0xB0, 0xB3, EPCPaintFinishKind::Default,
      UPCSwatchDesc_RP_LiquidDeuteriumTritiumBlend::StaticClass(),
      UPCSwatchRecipe_RP_LiquidDeuteriumTritiumBlend::StaticClass(), EPCFluidRosterSection::Rp);
  Add(TEXT("/RefinedPower/Resource/RawResources/LiquidTritium/"
           "Desc_RP_LiquidTritium.Desc_RP_LiquidTritium_C"),
      TEXT("RP_LiquidTritium"), 0xB3, 0xB3, 0xB3, EPCPaintFinishKind::Default,
      UPCSwatchDesc_RP_LiquidTritium::StaticClass(),
      UPCSwatchRecipe_RP_LiquidTritium::StaticClass(), EPCFluidRosterSection::Rp);
  Add(TEXT("/RefinedPower/Resource/RawResources/LowSteam/Desc_RP_LowSteam.Desc_RP_LowSteam_C"),
      TEXT("RP_LowSteam"), 0x9A, 0x9A, 0x9A, EPCPaintFinishKind::MetallicColor,
      UPCSwatchDesc_RP_LowSteam::StaticClass(), UPCSwatchRecipe_RP_LowSteam::StaticClass(),
      EPCFluidRosterSection::Rp);
  Add(TEXT("/RefinedPower/Resource/RawResources/MethaneGas/"
           "Desc_RP_MethaneGas.Desc_RP_MethaneGas_C"),
      TEXT("RP_MethaneGas"), 0xB6, 0xEB, 0xFF, EPCPaintFinishKind::MetallicColor,
      UPCSwatchDesc_RP_MethaneGas::StaticClass(), UPCSwatchRecipe_RP_MethaneGas::StaticClass(),
      EPCFluidRosterSection::Rp);
  Add(TEXT("/RefinedPower/Resource/RawResources/MoltenSalt/"
           "Desc_RP_MoltenSalt.Desc_RP_MoltenSalt_C"),
      TEXT("RP_MoltenSalt"), 0x00, 0x61, 0xFF, EPCPaintFinishKind::MetallicColor,
      UPCSwatchDesc_RP_MoltenSalt::StaticClass(), UPCSwatchRecipe_RP_MoltenSalt::StaticClass(),
      EPCFluidRosterSection::Rp);
  Add(TEXT("/RefinedPower/Resource/RawResources/ReactorCoolant/"
           "Desc_RP_ReactorCoolant.Desc_RP_ReactorCoolant_C"),
      TEXT("RP_ReactorCoolant"), 0x08, 0xBF, 0x48, EPCPaintFinishKind::Default,
      UPCSwatchDesc_RP_ReactorCoolant::StaticClass(),
      UPCSwatchRecipe_RP_ReactorCoolant::StaticClass(), EPCFluidRosterSection::Rp);
  Add(TEXT("/RefinedPower/Resource/RawResources/ToxicGas/Desc_RP_ToxicGas.Desc_RP_ToxicGas_C"),
      TEXT("RP_ToxicGas"), 0x17, 0x5A, 0x2D, EPCPaintFinishKind::MetallicColor,
      UPCSwatchDesc_RP_ToxicGas::StaticClass(), UPCSwatchRecipe_RP_ToxicGas::StaticClass(),
      EPCFluidRosterSection::Rp);

  Add(TEXT("/KLib/Assets/Ressources/Raw/Gas/Desc_Air.Desc_Air_C"), TEXT("Air"), 0x8C, 0xA0, 0xA0,
      EPCPaintFinishKind::MetallicColor, UPCSwatchDesc_Air::StaticClass(),
      UPCSwatchRecipe_Air::StaticClass(), EPCFluidRosterSection::Sfp);
  Add(TEXT("/KLib/Assets/Ressources/Raw/Fluids/Desc_CausticSoda.Desc_CausticSoda_C"),
      TEXT("CausticSoda"), 0x96, 0xC8, 0x96, EPCPaintFinishKind::Default,
      UPCSwatchDesc_CausticSoda::StaticClass(), UPCSwatchRecipe_CausticSoda::StaticClass(),
      EPCFluidRosterSection::Sfp);
  Add(TEXT("/KLib/Assets/Ressources/Raw/Fluids/Desc_CoolingWater.Desc_CoolingWater_C"),
      TEXT("CoolingWater"), 0x0A, 0xE3, 0xB1, EPCPaintFinishKind::Default,
      UPCSwatchDesc_CoolingWater::StaticClass(), UPCSwatchRecipe_CoolingWater::StaticClass(),
      EPCFluidRosterSection::Sfp);
  Add(TEXT("/KLib/Assets/Ressources/Raw/Fluids/Desc_DieselOil.Desc_DieselOil_C"), TEXT("DieselOil"),
      0xF4, 0xD6, 0x5D, EPCPaintFinishKind::Default, UPCSwatchDesc_DieselOil::StaticClass(),
      UPCSwatchRecipe_DieselOil::StaticClass(), EPCFluidRosterSection::Sfp);
  Add(TEXT("/KLib/Assets/Ressources/Raw/Gas/Desc_Ethylene.Desc_Ethylene_C"), TEXT("Ethylene"), 0xFF,
      0x00, 0x00, EPCPaintFinishKind::MetallicColor, UPCSwatchDesc_Ethylene::StaticClass(),
      UPCSwatchRecipe_Ethylene::StaticClass(), EPCFluidRosterSection::Sfp);
  Add(TEXT("/KLib/Assets/Ressources/Raw/Gas/Desc_FlueGas.Desc_FlueGas_C"), TEXT("FlueGas"), 0x96,
      0x96, 0x82, EPCPaintFinishKind::MetallicColor, UPCSwatchDesc_FlueGas::StaticClass(),
      UPCSwatchRecipe_FlueGas::StaticClass(), EPCFluidRosterSection::Sfp);
  Add(TEXT("/KLib/Assets/Ressources/Raw/Fluids/Desc_Fluid_RocketFuel.Desc_Fluid_RocketFuel_C"),
      TEXT("Fluid_RocketFuel"), 0x4B, 0x1B, 0x1E, EPCPaintFinishKind::Default,
      UPCSwatchDesc_Fluid_RocketFuel::StaticClass(),
      UPCSwatchRecipe_Fluid_RocketFuel::StaticClass(), EPCFluidRosterSection::Sfp);
  Add(TEXT("/KLib/Assets/Ressources/Raw/Gas/Desc_Gas_Chlor.Desc_Gas_Chlor_C"), TEXT("Gas_Chlor"),
      0xB4, 0xFF, 0x96, EPCPaintFinishKind::MetallicColor, UPCSwatchDesc_Gas_Chlor::StaticClass(),
      UPCSwatchRecipe_Gas_Chlor::StaticClass(), EPCFluidRosterSection::Sfp);
  Add(TEXT("/KLib/Assets/Ressources/Raw/Gas/Desc_Gas_Hydrogen.Desc_Gas_Hydrogen_C"),
      TEXT("Gas_Hydrogen"), 0xFF, 0xD7, 0x87, EPCPaintFinishKind::MetallicColor,
      UPCSwatchDesc_Gas_Hydrogen::StaticClass(), UPCSwatchRecipe_Gas_Hydrogen::StaticClass(),
      EPCFluidRosterSection::Sfp);
  Add(TEXT("/KLib/Assets/Ressources/Raw/Gas/Desc_Gas_Oxyhydrogen.Desc_Gas_Oxyhydrogen_C"),
      TEXT("Gas_Oxyhydrogen"), 0xEC, 0xD3, 0xB0, EPCPaintFinishKind::MetallicColor,
      UPCSwatchDesc_Gas_Oxyhydrogen::StaticClass(), UPCSwatchRecipe_Gas_Oxyhydrogen::StaticClass(),
      EPCFluidRosterSection::Sfp);
  Add(TEXT("/KLib/Assets/Ressources/Raw/Gas/Desc_Gas_Steam.Desc_Gas_Steam_C"), TEXT("Gas_Steam"),
      0x50, 0x80, 0xA0, EPCPaintFinishKind::MetallicColor, UPCSwatchDesc_Gas_Steam::StaticClass(),
      UPCSwatchRecipe_Gas_Steam::StaticClass(), EPCFluidRosterSection::Sfp);
  Add(TEXT("/KLib/Assets/Ressources/Raw/Gas/Desc_Gas_Steam_Hot.Desc_Gas_Steam_Hot_C"),
      TEXT("Gas_Steam_Hot"), 0xE1, 0xE1, 0xE1, EPCPaintFinishKind::MetallicColor,
      UPCSwatchDesc_Gas_Steam_Hot::StaticClass(), UPCSwatchRecipe_Gas_Steam_Hot::StaticClass(),
      EPCFluidRosterSection::Sfp);
  Add(TEXT("/KLib/Assets/Ressources/Raw/Gas/"
           "Desc_Gas_Titantetrachlorid.Desc_Gas_Titantetrachlorid_C"),
      TEXT("Gas_Titantetrachlorid"), 0xFF, 0xDC, 0xFF, EPCPaintFinishKind::MetallicColor,
      UPCSwatchDesc_Gas_Titantetrachlorid::StaticClass(),
      UPCSwatchRecipe_Gas_Titantetrachlorid::StaticClass(), EPCFluidRosterSection::Sfp);
  Add(TEXT("/KLib/Assets/Ressources/Raw/Gas/Desc_KLIB_NaturalGas.Desc_KLIB_NaturalGas_C"),
      TEXT("KLIB_NaturalGas"), 0xFF, 0x82, 0x00, EPCPaintFinishKind::MetallicColor,
      UPCSwatchDesc_KLIB_NaturalGas::StaticClass(), UPCSwatchRecipe_KLIB_NaturalGas::StaticClass(),
      EPCFluidRosterSection::Sfp);
  Add(TEXT("/KLib/Assets/Ressources/Raw/Fluids/Metals/Desc_LiquidAluminum.Desc_LiquidAluminum_C"),
      TEXT("LiquidAluminum"), 0x9C, 0xA0, 0xA0, EPCPaintFinishKind::Default,
      UPCSwatchDesc_LiquidAluminum::StaticClass(), UPCSwatchRecipe_LiquidAluminum::StaticClass(),
      EPCFluidRosterSection::Sfp);
  Add(TEXT("/KLib/Assets/Ressources/Raw/Fluids/Metals/Desc_LiquidBrass.Desc_LiquidBrass_C"),
      TEXT("LiquidBrass"), 0xDC, 0x87, 0x50, EPCPaintFinishKind::Default,
      UPCSwatchDesc_LiquidBrass::StaticClass(), UPCSwatchRecipe_LiquidBrass::StaticClass(),
      EPCFluidRosterSection::Sfp);
  Add(TEXT("/KLib/Assets/Ressources/Raw/Fluids/Metals/Desc_LiquidBronze.Desc_LiquidBronze_C"),
      TEXT("LiquidBronze"), 0x9B, 0x75, 0x37, EPCPaintFinishKind::Default,
      UPCSwatchDesc_LiquidBronze::StaticClass(), UPCSwatchRecipe_LiquidBronze::StaticClass(),
      EPCFluidRosterSection::Sfp);
  Add(TEXT("/KLib/Assets/Ressources/Raw/Fluids/Metals/Desc_LiquidCobalt.Desc_LiquidCobalt_C"),
      TEXT("LiquidCobalt"), 0x28, 0x28, 0x78, EPCPaintFinishKind::Default,
      UPCSwatchDesc_LiquidCobalt::StaticClass(), UPCSwatchRecipe_LiquidCobalt::StaticClass(),
      EPCFluidRosterSection::Sfp);
  Add(TEXT("/KLib/Assets/Ressources/Raw/Fluids/Metals/Desc_LiquidCopper.Desc_LiquidCopper_C"),
      TEXT("LiquidCopper"), 0xC8, 0x3C, 0x14, EPCPaintFinishKind::Default,
      UPCSwatchDesc_LiquidCopper::StaticClass(), UPCSwatchRecipe_LiquidCopper::StaticClass(),
      EPCFluidRosterSection::Sfp);
  Add(TEXT("/KLib/Assets/Ressources/Raw/Fluids/Metals/Desc_LiquidGold.Desc_LiquidGold_C"),
      TEXT("LiquidGold"), 0xA0, 0x8C, 0x32, EPCPaintFinishKind::Default,
      UPCSwatchDesc_LiquidGold::StaticClass(), UPCSwatchRecipe_LiquidGold::StaticClass(),
      EPCFluidRosterSection::Sfp);
  Add(TEXT("/KLib/Assets/Ressources/Raw/Fluids/"
           "Desc_LiquidHydrochloricAcid.Desc_LiquidHydrochloricAcid_C"),
      TEXT("LiquidHydrochloricAcid"), 0xB9, 0x78, 0x78, EPCPaintFinishKind::Default,
      UPCSwatchDesc_LiquidHydrochloricAcid::StaticClass(),
      UPCSwatchRecipe_LiquidHydrochloricAcid::StaticClass(), EPCFluidRosterSection::Sfp);
  Add(TEXT("/KLib/Assets/Ressources/Raw/Fluids/Metals/Desc_LiquidInvar.Desc_LiquidInvar_C"),
      TEXT("LiquidInvar"), 0x46, 0x50, 0x5A, EPCPaintFinishKind::Default,
      UPCSwatchDesc_LiquidInvar::StaticClass(), UPCSwatchRecipe_LiquidInvar::StaticClass(),
      EPCFluidRosterSection::Sfp);
  Add(TEXT("/KLib/Assets/Ressources/Raw/Fluids/Metals/Desc_LiquidIron.Desc_LiquidIron_C"),
      TEXT("LiquidIron"), 0x64, 0x55, 0x50, EPCPaintFinishKind::Default,
      UPCSwatchDesc_LiquidIron::StaticClass(), UPCSwatchRecipe_LiquidIron::StaticClass(),
      EPCFluidRosterSection::Sfp);
  Add(TEXT("/KLib/Assets/Ressources/Raw/Fluids/Metals/Desc_LiquidLead.Desc_LiquidLead_C"),
      TEXT("LiquidLead"), 0x5A, 0x46, 0x8C, EPCPaintFinishKind::Default,
      UPCSwatchDesc_LiquidLead::StaticClass(), UPCSwatchRecipe_LiquidLead::StaticClass(),
      EPCFluidRosterSection::Sfp);
  Add(TEXT("/KLib/Assets/Ressources/Raw/Fluids/Metals/"
           "Desc_LiquidMagnesium.Desc_LiquidMagnesium_C"),
      TEXT("LiquidMagnesium"), 0x7A, 0x89, 0x93, EPCPaintFinishKind::Default,
      UPCSwatchDesc_LiquidMagnesium::StaticClass(), UPCSwatchRecipe_LiquidMagnesium::StaticClass(),
      EPCFluidRosterSection::Sfp);
  Add(TEXT("/KLib/Assets/Ressources/Raw/Fluids/Metals/Desc_LiquidNickle.Desc_LiquidNickle_C"),
      TEXT("LiquidNickle"), 0x87, 0x6E, 0x50, EPCPaintFinishKind::Default,
      UPCSwatchDesc_LiquidNickle::StaticClass(), UPCSwatchRecipe_LiquidNickle::StaticClass(),
      EPCFluidRosterSection::Sfp);
  Add(TEXT("/KLib/Assets/Ressources/Raw/Fluids/Desc_LiquidRawSludge.Desc_LiquidRawSludge_C"),
      TEXT("LiquidRawSludge"), 0xB0, 0xA7, 0xA0, EPCPaintFinishKind::Default,
      UPCSwatchDesc_LiquidRawSludge::StaticClass(), UPCSwatchRecipe_LiquidRawSludge::StaticClass(),
      EPCFluidRosterSection::Sfp);
  Add(TEXT("/KLib/Assets/Ressources/Raw/Fluids/Metals/Desc_LiquidSlag.Desc_LiquidSlag_C"),
      TEXT("LiquidSlag"), 0x4B, 0x46, 0x23, EPCPaintFinishKind::Default,
      UPCSwatchDesc_LiquidSlag::StaticClass(), UPCSwatchRecipe_LiquidSlag::StaticClass(),
      EPCFluidRosterSection::Sfp);
  Add(TEXT("/KLib/Assets/Ressources/Raw/Fluids/Metals/Desc_LiquidSolder.Desc_LiquidSolder_C"),
      TEXT("LiquidSolder"), 0x8C, 0x82, 0xB4, EPCPaintFinishKind::Default,
      UPCSwatchDesc_LiquidSolder::StaticClass(), UPCSwatchRecipe_LiquidSolder::StaticClass(),
      EPCFluidRosterSection::Sfp);
  Add(TEXT("/KLib/Assets/Ressources/Raw/Fluids/Metals/Desc_LiquidSteel.Desc_LiquidSteel_C"),
      TEXT("LiquidSteel"), 0x3C, 0x3C, 0x41, EPCPaintFinishKind::Default,
      UPCSwatchDesc_LiquidSteel::StaticClass(), UPCSwatchRecipe_LiquidSteel::StaticClass(),
      EPCFluidRosterSection::Sfp);
  Add(TEXT("/KLib/Assets/Ressources/Raw/Fluids/Metals/Desc_LiquidTin.Desc_LiquidTin_C"),
      TEXT("LiquidTin"), 0x28, 0x5A, 0x96, EPCPaintFinishKind::Default,
      UPCSwatchDesc_LiquidTin::StaticClass(), UPCSwatchRecipe_LiquidTin::StaticClass(),
      EPCFluidRosterSection::Sfp);
  Add(TEXT("/KLib/Assets/Ressources/Raw/Fluids/Metals/Desc_LiquidZinc.Desc_LiquidZinc_C"),
      TEXT("LiquidZinc"), 0x4B, 0x78, 0x4B, EPCPaintFinishKind::Default,
      UPCSwatchDesc_LiquidZinc::StaticClass(), UPCSwatchRecipe_LiquidZinc::StaticClass(),
      EPCFluidRosterSection::Sfp);
  Add(TEXT("/KLib/Assets/Ressources/Raw/Fluids/Desc_Naphtha.Desc_Naphtha_C"), TEXT("Naphtha"), 0xF0,
      0x6E, 0x6E, EPCPaintFinishKind::Default, UPCSwatchDesc_Naphtha::StaticClass(),
      UPCSwatchRecipe_Naphtha::StaticClass(), EPCFluidRosterSection::Sfp);
  Add(TEXT("/KLib/Assets/Ressources/Raw/Gas/Desc_Neutronium.Desc_Neutronium_C"), TEXT("Neutronium"),
      0x00, 0x00, 0x5A, EPCPaintFinishKind::MetallicColor, UPCSwatchDesc_Neutronium::StaticClass(),
      UPCSwatchRecipe_Neutronium::StaticClass(), EPCFluidRosterSection::Sfp);
  Add(TEXT("/KLib/Assets/Ressources/Raw/Fluids/Desc_Paint.Desc_Paint_C"), TEXT("Paint"), 0xC4, 0xC4,
      0xFF, EPCPaintFinishKind::Default, UPCSwatchDesc_Paint::StaticClass(),
      UPCSwatchRecipe_Paint::StaticClass(), EPCFluidRosterSection::Sfp);
  Add(TEXT("/KLib/Assets/Ressources/Raw/Fluids/Desc_Paraffin.Desc_Paraffin_C"), TEXT("Paraffin"),
      0xBE, 0x83, 0x4A, EPCPaintFinishKind::Default, UPCSwatchDesc_Paraffin::StaticClass(),
      UPCSwatchRecipe_Paraffin::StaticClass(), EPCFluidRosterSection::Sfp);
  Add(TEXT("/KLib/Assets/Ressources/Raw/Fluids/Desc_PurifiedCrudeOil.Desc_PurifiedCrudeOil_C"),
      TEXT("PurifiedCrudeOil"), 0x32, 0x19, 0x23, EPCPaintFinishKind::Default,
      UPCSwatchDesc_PurifiedCrudeOil::StaticClass(),
      UPCSwatchRecipe_PurifiedCrudeOil::StaticClass(), EPCFluidRosterSection::Sfp);
  Add(TEXT("/KLib/Assets/Ressources/Raw/Fluids/Slugs/Desc_SlugSlime_High.Desc_SlugSlime_High_C"),
      TEXT("SlugSlime_High"), 0xFF, 0x3C, 0xDC, EPCPaintFinishKind::Default,
      UPCSwatchDesc_SlugSlime_High::StaticClass(), UPCSwatchRecipe_SlugSlime_High::StaticClass(),
      EPCFluidRosterSection::Sfp);
  Add(TEXT("/KLib/Assets/Ressources/Raw/Fluids/Slugs/Desc_SlugSlime_Low.Desc_SlugSlime_Low_C"),
      TEXT("SlugSlime_Low"), 0xA0, 0x73, 0x8C, EPCPaintFinishKind::Default,
      UPCSwatchDesc_SlugSlime_Low::StaticClass(), UPCSwatchRecipe_SlugSlime_Low::StaticClass(),
      EPCFluidRosterSection::Sfp);
  Add(TEXT("/KLib/Assets/Ressources/Raw/Fluids/Slugs/Desc_SlugSlime_Mid.Desc_SlugSlime_Mid_C"),
      TEXT("SlugSlime_Mid"), 0x2D, 0x23, 0x2D, EPCPaintFinishKind::Default,
      UPCSwatchDesc_SlugSlime_Mid::StaticClass(), UPCSwatchRecipe_SlugSlime_Mid::StaticClass(),
      EPCFluidRosterSection::Sfp);
  Add(TEXT("/KLib/Assets/Ressources/Raw/Fluids/Slugs/"
           "Desc_SlugSlime_Mineral.Desc_SlugSlime_Mineral_C"),
      TEXT("SlugSlime_Mineral"), 0x80, 0x00, 0xFF, EPCPaintFinishKind::Default,
      UPCSwatchDesc_SlugSlime_Mineral::StaticClass(),
      UPCSwatchRecipe_SlugSlime_Mineral::StaticClass(), EPCFluidRosterSection::Sfp);
  Add(TEXT("/KLib/Assets/Ressources/Raw/Gas/Desc_Sulfurtrixid.Desc_Sulfurtrixid_C"),
      TEXT("Sulfurtrixid"), 0xE1, 0xD7, 0x32, EPCPaintFinishKind::MetallicColor,
      UPCSwatchDesc_Sulfurtrixid::StaticClass(), UPCSwatchRecipe_Sulfurtrixid::StaticClass(),
      EPCFluidRosterSection::Sfp);
  Add(TEXT("/KLib/Assets/Ressources/Raw/Fluids/Metals/Desc_TiHy.Desc_TiHy_C"), TEXT("TiHy"), 0x84,
      0x76, 0x80, EPCPaintFinishKind::Default, UPCSwatchDesc_TiHy::StaticClass(),
      UPCSwatchRecipe_TiHy::StaticClass(), EPCFluidRosterSection::Sfp);
  Add(TEXT("/KLib/Assets/Ressources/Raw/Gas/Desc_ToxicAir.Desc_ToxicAir_C"), TEXT("ToxicAir"), 0xB4,
      0xFF, 0x3C, EPCPaintFinishKind::MetallicColor, UPCSwatchDesc_ToxicAir::StaticClass(),
      UPCSwatchRecipe_ToxicAir::StaticClass(), EPCFluidRosterSection::Sfp);
  Add(TEXT("/KLib/Assets/Ressources/Raw/Fluids/Desc_UranChloride.Desc_UranChloride_C"),
      TEXT("UranChloride"), 0x50, 0x96, 0x14, EPCPaintFinishKind::MetallicColor,
      UPCSwatchDesc_UranChloride::StaticClass(), UPCSwatchRecipe_UranChloride::StaticClass(),
      EPCFluidRosterSection::Sfp);
  Add(TEXT("/KLib/Assets/Ressources/Raw/Fluids/Desc_Wastewater.Desc_Wastewater_C"),
      TEXT("Wastewater"), 0x26, 0x0C, 0x00, EPCPaintFinishKind::Default,
      UPCSwatchDesc_Wastewater::StaticClass(), UPCSwatchRecipe_Wastewater::StaticClass(),
      EPCFluidRosterSection::Sfp);

  return Rows;
}

bool SoftDescPresent(const FPCFluidRosterRow& Row) {
  if (!Row.SoftPath) {
    return false;
  }
  return FSoftClassPath(Row.SoftPath).TryLoadClass<UFGItemDescriptor>() != nullptr;
}

void AppendAlwaysSwatchClasses(TArray<TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch>>& Out) {
  Out.Add(UPCSwatchDesc_Neutral::StaticClass());
  for (const FPCFluidRosterRow& Row : FluidRows()) {
    if (Row.Section == EPCFluidRosterSection::Default) {
      Out.Add(Row.SwatchClass);
    }
  }
  Out.Add(UPCSwatchDesc_Fallback::StaticClass());
}

void AppendAlwaysRecipeClasses(TArray<TSubclassOf<UFGCustomizationRecipe>>& Out) {
  Out.Add(UPCSwatchRecipe_Neutral::StaticClass());
  for (const FPCFluidRosterRow& Row : FluidRows()) {
    if (Row.Section == EPCFluidRosterSection::Default) {
      Out.AddUnique(Row.RecipeClass);
    }
  }
  Out.Add(UPCSwatchRecipe_Fallback::StaticClass());
}

void AppendAvailableModSwatchClasses(
    TArray<TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch>>& Out,
    const TSet<FName>& AvailableStems) {
  for (const FPCFluidRosterRow& Row : FluidRows()) {
    if (Row.Section == EPCFluidRosterSection::Default) {
      continue;
    }
    if (AvailableStems.Contains(Row.Stem)) {
      Out.Add(Row.SwatchClass);
    }
  }
}

void AppendAvailableModRecipeClasses(TArray<TSubclassOf<UFGCustomizationRecipe>>& Out,
                                     const TSet<FName>& AvailableStems) {
  for (const FPCFluidRosterRow& Row : FluidRows()) {
    if (Row.Section == EPCFluidRosterSection::Default) {
      continue;
    }
    if (AvailableStems.Contains(Row.Stem)) {
      Out.AddUnique(Row.RecipeClass);
    }
  }
}

void AppendAllSwatchClasses(TArray<TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch>>& Out) {
  AppendAlwaysSwatchClasses(Out);
  for (const FPCFluidRosterRow& Row : FluidRows()) {
    if (Row.Section != EPCFluidRosterSection::Default) {
      Out.Add(Row.SwatchClass);
    }
  }
}

void AppendAllRecipeClasses(TArray<TSubclassOf<UFGCustomizationRecipe>>& Out) {
  AppendAlwaysRecipeClasses(Out);
  for (const FPCFluidRosterRow& Row : FluidRows()) {
    if (Row.Section != EPCFluidRosterSection::Default) {
      Out.AddUnique(Row.RecipeClass);
    }
  }
}

} // namespace FPCFluidRoster
