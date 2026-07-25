// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "FGCustomizationRecipe.h"
#include "FGFactoryColoringTypes.h"
#include "UPCSwatchRecipes.generated.h"

UCLASS(Abstract)
class PIPELINECOLOR_API UPCSwatchRecipeBase : public UFGCustomizationRecipe {
  GENERATED_BODY()

 public:
  void InitRecipe(TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Desc,
                  const TCHAR* DisplayName);
};

UCLASS()
class PIPELINECOLOR_API UPCSwatchRecipe_Neutral : public UPCSwatchRecipeBase {
  GENERATED_BODY()
 public:
  UPCSwatchRecipe_Neutral();
};

UCLASS()
class PIPELINECOLOR_API UPCSwatchRecipe_Fallback : public UPCSwatchRecipeBase {
  GENERATED_BODY()
 public:
  UPCSwatchRecipe_Fallback();
};
