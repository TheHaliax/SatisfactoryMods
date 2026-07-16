// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "FGFactoryColoringTypes.h"
#include "UPCSwatchDescs.generated.h"

/**
 * ID is always INDEX_CUSTOM_COLOR_SLOT (255), never a GameState color-slot index.
 */
UCLASS(Abstract)
class PIPELINECOLOR_API UPCSwatchDescBase : public UFGFactoryCustomizationDescriptor_Swatch {
  GENERATED_BODY()

 public:
  UPCSwatchDescBase();

  UPROPERTY()
  FName CatalogKey = NAME_None;

  static FName GetCatalogKey(TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Swatch);
};

UCLASS()
class PIPELINECOLOR_API UPCSwatchDesc_Neutral : public UPCSwatchDescBase {
  GENERATED_BODY()
 public:
  UPCSwatchDesc_Neutral();
};

UCLASS()
class PIPELINECOLOR_API UPCSwatchDesc_Water : public UPCSwatchDescBase {
  GENERATED_BODY()
 public:
  UPCSwatchDesc_Water();
};

UCLASS()
class PIPELINECOLOR_API UPCSwatchDesc_CrudeOil : public UPCSwatchDescBase {
  GENERATED_BODY()
 public:
  UPCSwatchDesc_CrudeOil();
};

UCLASS()
class PIPELINECOLOR_API UPCSwatchDesc_HeavyOilResidue : public UPCSwatchDescBase {
  GENERATED_BODY()
 public:
  UPCSwatchDesc_HeavyOilResidue();
};

UCLASS()
class PIPELINECOLOR_API UPCSwatchDesc_Fuel : public UPCSwatchDescBase {
  GENERATED_BODY()
 public:
  UPCSwatchDesc_Fuel();
};

UCLASS()
class PIPELINECOLOR_API UPCSwatchDesc_Turbofuel : public UPCSwatchDescBase {
  GENERATED_BODY()
 public:
  UPCSwatchDesc_Turbofuel();
};

UCLASS()
class PIPELINECOLOR_API UPCSwatchDesc_LiquidBiofuel : public UPCSwatchDescBase {
  GENERATED_BODY()
 public:
  UPCSwatchDesc_LiquidBiofuel();
};

UCLASS()
class PIPELINECOLOR_API UPCSwatchDesc_AluminaSolution : public UPCSwatchDescBase {
  GENERATED_BODY()
 public:
  UPCSwatchDesc_AluminaSolution();
};

UCLASS()
class PIPELINECOLOR_API UPCSwatchDesc_SulfuricAcid : public UPCSwatchDescBase {
  GENERATED_BODY()
 public:
  UPCSwatchDesc_SulfuricAcid();
};

UCLASS()
class PIPELINECOLOR_API UPCSwatchDesc_DissolvedSilica : public UPCSwatchDescBase {
  GENERATED_BODY()
 public:
  UPCSwatchDesc_DissolvedSilica();
};

UCLASS()
class PIPELINECOLOR_API UPCSwatchDesc_NitricAcid : public UPCSwatchDescBase {
  GENERATED_BODY()
 public:
  UPCSwatchDesc_NitricAcid();
};

UCLASS()
class PIPELINECOLOR_API UPCSwatchDesc_DarkMatterResidue : public UPCSwatchDescBase {
  GENERATED_BODY()
 public:
  UPCSwatchDesc_DarkMatterResidue();
};

UCLASS()
class PIPELINECOLOR_API UPCSwatchDesc_ExcitedPhotonicMatter : public UPCSwatchDescBase {
  GENERATED_BODY()
 public:
  UPCSwatchDesc_ExcitedPhotonicMatter();
};

UCLASS()
class PIPELINECOLOR_API UPCSwatchDesc_IonizedFuel : public UPCSwatchDescBase {
  GENERATED_BODY()
 public:
  UPCSwatchDesc_IonizedFuel();
};

UCLASS()
class PIPELINECOLOR_API UPCSwatchDesc_RocketFuel : public UPCSwatchDescBase {
  GENERATED_BODY()
 public:
  UPCSwatchDesc_RocketFuel();
};

UCLASS()
class PIPELINECOLOR_API UPCSwatchDesc_NitrogenGas : public UPCSwatchDescBase {
  GENERATED_BODY()
 public:
  UPCSwatchDesc_NitrogenGas();
};

UCLASS()
class PIPELINECOLOR_API UPCSwatchDesc_Fallback : public UPCSwatchDescBase {
  GENERATED_BODY()
 public:
  UPCSwatchDesc_Fallback();
};
