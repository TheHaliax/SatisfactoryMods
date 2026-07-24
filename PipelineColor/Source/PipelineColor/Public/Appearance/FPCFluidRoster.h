// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "Appearance/FPCFluidAppearanceCatalog.h"
#include "CoreMinimal.h"
#include "FGCustomizationRecipe.h"
#include "FGFactoryColoringTypes.h"
#include "Templates/SubclassOf.h"

enum class EPCFluidRosterSection : uint8 {
  Default,
  Sfp,
  Rp,
};

struct FPCFluidRosterRow {
  const TCHAR* SoftPath = nullptr;
  FName Stem = NAME_None;
  uint8 PrimaryR = 0;
  uint8 PrimaryG = 0;
  uint8 PrimaryB = 0;
  EPCPaintFinishKind Finish = EPCPaintFinishKind::Default;
  TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> SwatchClass;
  TSubclassOf<UFGCustomizationRecipe> RecipeClass;
  EPCFluidRosterSection Section = EPCFluidRosterSection::Default;
};

namespace FPCFluidRoster {
const TArray<FPCFluidRosterRow>& FluidRows();

bool SoftDescPresent(const FPCFluidRosterRow& Row);

void AppendAllSwatchClasses(TArray<TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch>>& Out);
void AppendAllRecipeClasses(TArray<TSubclassOf<UFGCustomizationRecipe>>& Out);

void AppendAlwaysSwatchClasses(TArray<TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch>>& Out);
void AppendAlwaysRecipeClasses(TArray<TSubclassOf<UFGCustomizationRecipe>>& Out);

void AppendAvailableModSwatchClasses(
    TArray<TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch>>& Out,
    const TSet<FName>& AvailableStems);
void AppendAvailableModRecipeClasses(TArray<TSubclassOf<UFGCustomizationRecipe>>& Out,
                                     const TSet<FName>& AvailableStems);
} // namespace FPCFluidRoster
