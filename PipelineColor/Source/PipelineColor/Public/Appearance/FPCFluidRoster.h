// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "Appearance/FPCFluidAppearanceCatalog.h"
#include "CoreMinimal.h"
#include "FGCustomizationRecipe.h"
#include "FGFactoryColoringTypes.h"
#include "Templates/SubclassOf.h"

struct FPCFluidRosterRow {
  const TCHAR* SoftPath = nullptr;
  FName Stem = NAME_None;
  uint8 PrimaryR = 0;
  uint8 PrimaryG = 0;
  uint8 PrimaryB = 0;
  EPCPaintFinishKind Finish = EPCPaintFinishKind::Default;
  TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> SwatchClass;
  TSubclassOf<UFGCustomizationRecipe> RecipeClass;
};

namespace FPCFluidRoster {
const TArray<FPCFluidRosterRow>& FluidRows();
void AppendAllSwatchClasses(TArray<TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch>>& Out);
void AppendAllRecipeClasses(TArray<TSubclassOf<UFGCustomizationRecipe>>& Out);
} // namespace FPCFluidRoster
