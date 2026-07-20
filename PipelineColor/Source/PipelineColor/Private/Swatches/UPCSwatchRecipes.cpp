// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Swatches/UPCSwatchRecipes.h"

#include "Swatches/UPCSwatchDescs.h"
#include "UObject/SoftObjectPath.h"

void UPCSwatchRecipeBase::InitRecipe(TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Desc,
                                     const TCHAR* DisplayName) {
  mDisplayNameOverride = true;
  mDisplayName = FText::FromString(DisplayName);
  mCustomizationDesc = Desc;
  mManufactoringDuration = 1.f;

  mProducedIn.Reset();
  mProducedIn.Add(TSoftClassPtr<UObject>(
      FSoftObjectPath(TEXT("/Game/FactoryGame/Equipment/BuildGun/BP_BuildGun.BP_BuildGun_C"))));
}

UPCSwatchRecipe_Neutral::UPCSwatchRecipe_Neutral() {
  InitRecipe(UPCSwatchDesc_Neutral::StaticClass(), TEXT("PC Neutral"));
}

UPCSwatchRecipe_Water::UPCSwatchRecipe_Water() {
  InitRecipe(UPCSwatchDesc_Water::StaticClass(), TEXT("PC Water"));
}

UPCSwatchRecipe_CrudeOil::UPCSwatchRecipe_CrudeOil() {
  InitRecipe(UPCSwatchDesc_CrudeOil::StaticClass(), TEXT("PC Crude Oil"));
}

UPCSwatchRecipe_HeavyOilResidue::UPCSwatchRecipe_HeavyOilResidue() {
  InitRecipe(UPCSwatchDesc_HeavyOilResidue::StaticClass(), TEXT("PC Heavy Oil Residue"));
}

UPCSwatchRecipe_Fuel::UPCSwatchRecipe_Fuel() {
  InitRecipe(UPCSwatchDesc_Fuel::StaticClass(), TEXT("PC Fuel"));
}

UPCSwatchRecipe_Turbofuel::UPCSwatchRecipe_Turbofuel() {
  InitRecipe(UPCSwatchDesc_Turbofuel::StaticClass(), TEXT("PC Turbofuel"));
}

UPCSwatchRecipe_LiquidBiofuel::UPCSwatchRecipe_LiquidBiofuel() {
  InitRecipe(UPCSwatchDesc_LiquidBiofuel::StaticClass(), TEXT("PC Liquid Biofuel"));
}

UPCSwatchRecipe_AluminaSolution::UPCSwatchRecipe_AluminaSolution() {
  InitRecipe(UPCSwatchDesc_AluminaSolution::StaticClass(), TEXT("PC Alumina Solution"));
}

UPCSwatchRecipe_SulfuricAcid::UPCSwatchRecipe_SulfuricAcid() {
  InitRecipe(UPCSwatchDesc_SulfuricAcid::StaticClass(), TEXT("PC Sulfuric Acid"));
}

UPCSwatchRecipe_DissolvedSilica::UPCSwatchRecipe_DissolvedSilica() {
  InitRecipe(UPCSwatchDesc_DissolvedSilica::StaticClass(), TEXT("PC Dissolved Silica"));
}

UPCSwatchRecipe_NitricAcid::UPCSwatchRecipe_NitricAcid() {
  InitRecipe(UPCSwatchDesc_NitricAcid::StaticClass(), TEXT("PC Nitric Acid"));
}

UPCSwatchRecipe_DarkMatterResidue::UPCSwatchRecipe_DarkMatterResidue() {
  InitRecipe(UPCSwatchDesc_DarkMatterResidue::StaticClass(), TEXT("PC Dark Matter Residue"));
}

UPCSwatchRecipe_ExcitedPhotonicMatter::UPCSwatchRecipe_ExcitedPhotonicMatter() {
  InitRecipe(UPCSwatchDesc_ExcitedPhotonicMatter::StaticClass(),
             TEXT("PC Excited Photonic Matter"));
}

UPCSwatchRecipe_IonizedFuel::UPCSwatchRecipe_IonizedFuel() {
  InitRecipe(UPCSwatchDesc_IonizedFuel::StaticClass(), TEXT("PC Ionized Fuel"));
}

UPCSwatchRecipe_RocketFuel::UPCSwatchRecipe_RocketFuel() {
  InitRecipe(UPCSwatchDesc_RocketFuel::StaticClass(), TEXT("PC Rocket Fuel"));
}

UPCSwatchRecipe_NitrogenGas::UPCSwatchRecipe_NitrogenGas() {
  InitRecipe(UPCSwatchDesc_NitrogenGas::StaticClass(), TEXT("PC Nitrogen Gas"));
}

UPCSwatchRecipe_Fallback::UPCSwatchRecipe_Fallback() {
  InitRecipe(UPCSwatchDesc_Fallback::StaticClass(), TEXT("PC Fallback"));
}
