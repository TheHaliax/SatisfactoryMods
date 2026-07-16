// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Appearance/FPCFluidRoster.h"

#include "Swatches/UPCSwatchDescs.h"
#include "Swatches/UPCSwatchRecipes.h"

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
                 TSubclassOf<UFGCustomizationRecipe> Recipe) {
    FPCFluidRosterRow Row;
    Row.SoftPath = Path;
    Row.Stem = FName(Stem);
    Row.PrimaryR = R;
    Row.PrimaryG = G;
    Row.PrimaryB = B;
    Row.Finish = Finish;
    Row.SwatchClass = Swatch;
    Row.RecipeClass = Recipe;
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

  return Rows;
}

void AppendAllSwatchClasses(TArray<TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch>>& Out) {
  Out.Add(UPCSwatchDesc_Neutral::StaticClass());
  for (const FPCFluidRosterRow& Row : FluidRows()) {
    Out.Add(Row.SwatchClass);
  }
  Out.Add(UPCSwatchDesc_Fallback::StaticClass());
}

void AppendAllRecipeClasses(TArray<TSubclassOf<UFGCustomizationRecipe>>& Out) {
  Out.Add(UPCSwatchRecipe_Neutral::StaticClass());
  for (const FPCFluidRosterRow& Row : FluidRows()) {
    Out.AddUnique(Row.RecipeClass);
  }
  Out.Add(UPCSwatchRecipe_Fallback::StaticClass());
}
}  // namespace FPCFluidRoster
