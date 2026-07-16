// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "FGFactoryColoringTypes.h"
#include "Resources/FGItemDescriptor.h"
#include "Templates/SubclassOf.h"

struct FPCAppearanceSpec {
  TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> SwatchDesc;
  FLinearColor PrimaryColor = FLinearColor(0.43f, 0.43f, 0.43f, 1.f);
  FLinearColor SecondaryColor = FLinearColor(0.15f, 0.15f, 0.15f, 1.f);
  TSubclassOf<UFGFactoryCustomizationDescriptor_PaintFinish> PaintFinish;
  FName CatalogKey = NAME_None;
  // Shared MetallicColor finish CDO — stamp Roughness before each paint.
  float RoughnessValue = 0.4f;
  bool bOverrideRoughness = false;

  bool IsValid() const {
    return SwatchDesc != nullptr || PaintFinish != nullptr;
  }

  bool EqualsApplied(const FPCAppearanceSpec& Other) const {
    return SwatchDesc == Other.SwatchDesc && PaintFinish == Other.PaintFinish &&
           CatalogKey == Other.CatalogKey && PrimaryColor.Equals(Other.PrimaryColor, 0.001f) &&
           SecondaryColor.Equals(Other.SecondaryColor, 0.001f) &&
           bOverrideRoughness == Other.bOverrideRoughness &&
           (!bOverrideRoughness ||
            FMath::IsNearlyEqual(RoughnessValue, Other.RoughnessValue, 0.001f));
  }
};
