// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "FGFactoryColoringTypes.h"
#include "FPCSwatchEntry.generated.h"

USTRUCT(BlueprintType)
struct PIPELINECOLOR_API FPCSwatchEntry {
  GENERATED_BODY()

  UPROPERTY(SaveGame, BlueprintReadWrite)
  FName Key = NAME_None;

  UPROPERTY(SaveGame, BlueprintReadWrite)
  FLinearColor Primary = FLinearColor(0.43f, 0.43f, 0.43f, 1.f);

  UPROPERTY(SaveGame, BlueprintReadWrite)
  FLinearColor Secondary = FLinearColor(0.15f, 0.15f, 0.15f, 1.f);

  UPROPERTY(SaveGame, BlueprintReadWrite)
  FSoftClassPath PaintFinish;

  FFactoryCustomizationColorSlot ToSlot() const {
    FFactoryCustomizationColorSlot Slot(Primary, Secondary);
    if (PaintFinish.IsValid()) {
      Slot.PaintFinish = PaintFinish.TryLoadClass<UFGFactoryCustomizationDescriptor_PaintFinish>();
    }
    return Slot;
  }

  void FromSlot(FName InKey, const FFactoryCustomizationColorSlot& Slot) {
    Key = InKey;
    Primary = Slot.PrimaryColor;
    Secondary = Slot.SecondaryColor;
    PaintFinish = Slot.PaintFinish ? FSoftClassPath(Slot.PaintFinish.Get()) : FSoftClassPath();
  }
};
