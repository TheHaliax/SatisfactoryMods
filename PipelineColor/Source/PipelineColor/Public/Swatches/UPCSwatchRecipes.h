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

 protected:
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
class PIPELINECOLOR_API UPCSwatchRecipe_Water : public UPCSwatchRecipeBase {
  GENERATED_BODY()
 public:
  UPCSwatchRecipe_Water();
};

UCLASS()
class PIPELINECOLOR_API UPCSwatchRecipe_CrudeOil : public UPCSwatchRecipeBase {
  GENERATED_BODY()
 public:
  UPCSwatchRecipe_CrudeOil();
};

UCLASS()
class PIPELINECOLOR_API UPCSwatchRecipe_HeavyOilResidue : public UPCSwatchRecipeBase {
  GENERATED_BODY()
 public:
  UPCSwatchRecipe_HeavyOilResidue();
};

UCLASS()
class PIPELINECOLOR_API UPCSwatchRecipe_Fuel : public UPCSwatchRecipeBase {
  GENERATED_BODY()
 public:
  UPCSwatchRecipe_Fuel();
};

UCLASS()
class PIPELINECOLOR_API UPCSwatchRecipe_Turbofuel : public UPCSwatchRecipeBase {
  GENERATED_BODY()
 public:
  UPCSwatchRecipe_Turbofuel();
};

UCLASS()
class PIPELINECOLOR_API UPCSwatchRecipe_LiquidBiofuel : public UPCSwatchRecipeBase {
  GENERATED_BODY()
 public:
  UPCSwatchRecipe_LiquidBiofuel();
};

UCLASS()
class PIPELINECOLOR_API UPCSwatchRecipe_AluminaSolution : public UPCSwatchRecipeBase {
  GENERATED_BODY()
 public:
  UPCSwatchRecipe_AluminaSolution();
};

UCLASS()
class PIPELINECOLOR_API UPCSwatchRecipe_SulfuricAcid : public UPCSwatchRecipeBase {
  GENERATED_BODY()
 public:
  UPCSwatchRecipe_SulfuricAcid();
};

UCLASS()
class PIPELINECOLOR_API UPCSwatchRecipe_DissolvedSilica : public UPCSwatchRecipeBase {
  GENERATED_BODY()
 public:
  UPCSwatchRecipe_DissolvedSilica();
};

UCLASS()
class PIPELINECOLOR_API UPCSwatchRecipe_NitricAcid : public UPCSwatchRecipeBase {
  GENERATED_BODY()
 public:
  UPCSwatchRecipe_NitricAcid();
};

UCLASS()
class PIPELINECOLOR_API UPCSwatchRecipe_DarkMatterResidue : public UPCSwatchRecipeBase {
  GENERATED_BODY()
 public:
  UPCSwatchRecipe_DarkMatterResidue();
};

UCLASS()
class PIPELINECOLOR_API UPCSwatchRecipe_ExcitedPhotonicMatter : public UPCSwatchRecipeBase {
  GENERATED_BODY()
 public:
  UPCSwatchRecipe_ExcitedPhotonicMatter();
};

UCLASS()
class PIPELINECOLOR_API UPCSwatchRecipe_IonizedFuel : public UPCSwatchRecipeBase {
  GENERATED_BODY()
 public:
  UPCSwatchRecipe_IonizedFuel();
};

UCLASS()
class PIPELINECOLOR_API UPCSwatchRecipe_RocketFuel : public UPCSwatchRecipeBase {
  GENERATED_BODY()
 public:
  UPCSwatchRecipe_RocketFuel();
};

UCLASS()
class PIPELINECOLOR_API UPCSwatchRecipe_NitrogenGas : public UPCSwatchRecipeBase {
  GENERATED_BODY()
 public:
  UPCSwatchRecipe_NitrogenGas();
};

UCLASS()
class PIPELINECOLOR_API UPCSwatchRecipe_Fallback : public UPCSwatchRecipeBase {
  GENERATED_BODY()
 public:
  UPCSwatchRecipe_Fallback();
};
