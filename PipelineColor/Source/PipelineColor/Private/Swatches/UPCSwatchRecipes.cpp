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
  InitRecipe(UPCSwatchDesc_Neutral::StaticClass(), TEXT("PC Empty Pipe"));
}

UPCSwatchRecipe_Fallback::UPCSwatchRecipe_Fallback() {
  InitRecipe(UPCSwatchDesc_Fallback::StaticClass(), TEXT("PC ERR0R"));
}
