// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "FGFactoryColoringTypes.h"
#include "UObject/SoftObjectPath.h"
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

  // SCIM-safe: Str property. SoftClassPath breaks AnthorNet SaveParser.
  UPROPERTY(SaveGame, BlueprintReadWrite)
  FString PaintFinishPath;

 private:
  // Resolve cache — transient, not serialized; weak so GC reload never dangles.
  mutable TWeakObjectPtr<UClass> CachedFinishClass;
  mutable FString CachedFinishPath;

 public:
  TSubclassOf<UFGFactoryCustomizationDescriptor_PaintFinish> GetPaintFinishClass() const {
    if (PaintFinishPath.IsEmpty()) {
      return nullptr;
    }

    // Weak cache per entry: overlay resolves run per pipe apply; re-parse and
    // TryLoad only when GC dropped the class or the path changed.
    if (CachedFinishPath == PaintFinishPath) {
      if (UClass* Cached = CachedFinishClass.Get()) {
        return Cached;
      }
    }

    UClass* Loaded = FSoftClassPath(PaintFinishPath)
                         .TryLoadClass<UFGFactoryCustomizationDescriptor_PaintFinish>();
    CachedFinishClass = Loaded;
    CachedFinishPath = PaintFinishPath;
    return Loaded;
  }

  void SetPaintFinishClass(TSubclassOf<UFGFactoryCustomizationDescriptor_PaintFinish> Finish) {
    // Slot.PaintFinish is live from Customizer — still TryLoad-safe path extract.
    // Never pass catalog-cached TSubclassOf here (GC can leave dangling UClass*).
    CachedFinishClass.Reset();
    CachedFinishPath.Reset();
    UClass* Cls = Finish.Get();
    if (!Cls) {
      PaintFinishPath.Reset();
      return;
    }
    PaintFinishPath = FSoftClassPath(Cls).ToString();
  }

  FFactoryCustomizationColorSlot ToSlot() const {
    FFactoryCustomizationColorSlot Slot(Primary, Secondary);
    Slot.PaintFinish = GetPaintFinishClass();
    return Slot;
  }

  void FromSlot(FName InKey, const FFactoryCustomizationColorSlot& Slot) {
    Key = InKey;
    Primary = Slot.PrimaryColor;
    Secondary = Slot.SecondaryColor;
    SetPaintFinishClass(Slot.PaintFinish);
  }
};
