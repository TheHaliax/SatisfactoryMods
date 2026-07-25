// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "Appearance/FPCAppearanceSpec.h"
#include "Appearance/IAppearanceCatalog.h"
#include "CoreMinimal.h"
#include "Resources/FGItemDescriptor.h"
#include "Templates/SubclassOf.h"

enum class EPCPaintFinishKind : uint8 {
  Default,
  Matte,
  MetallicColor,
};

class FPCFluidAppearanceCatalog final : public IAppearanceCatalog {
 public:
  static FPCFluidAppearanceCatalog& Get();

  void EnsureLoaded() const;
  void Invalidate() const;

  virtual bool Resolve(TSubclassOf<UFGItemDescriptor> FluidDescriptor, bool bEmpty,
                       FPCAppearanceSpec& OutSpec) const override;

  const FPCAppearanceSpec& GetNeutral() const {
    return NeutralSpec;
  }

  bool ResolveByKey(FName CatalogKey, FPCAppearanceSpec& OutSpec) const;

  TSubclassOf<UFGFactoryCustomizationDescriptor_PaintFinish>
  GetFinishClass(EPCPaintFinishKind Kind) const;

  static FString GetFinishPath(EPCPaintFinishKind Kind);

  EPCPaintFinishKind FinishKindForKey(FName CatalogKey) const;
  bool IsGasCatalogKey(FName CatalogKey) const;

 private:
  FPCFluidAppearanceCatalog() = default;

  void EnsureNeutral() const;
  void FillNeutralSpec(FPCAppearanceSpec& OutSpec) const;
  void FillFromDynamic(const struct FPCDynamicSwatchEntry& Entry, FPCAppearanceSpec& OutSpec) const;
  static FLinearColor HexRgb(uint8 R, uint8 G, uint8 B);
  static FLinearColor MissingMagenta();

  mutable bool bNeutralReady = false;
  mutable FPCAppearanceSpec NeutralSpec;
};
