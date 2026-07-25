// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "FGFactoryColoringTypes.h"
#include "UPCSwatchDescs.generated.h"

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
class PIPELINECOLOR_API UPCSwatchDesc_Fallback : public UPCSwatchDescBase {
  GENERATED_BODY()
 public:
  UPCSwatchDesc_Fallback();
};
