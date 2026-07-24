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

struct FPCFluidCatalogEntry {
  FName FluidStem;
  FString SoftPath;
  FLinearColor Primary;
  EPCPaintFinishKind Finish = EPCPaintFinishKind::Default;
  TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> SwatchClass;
};

class FPCFluidAppearanceCatalog final : public IAppearanceCatalog {
 public:
  static FPCFluidAppearanceCatalog& Get();

  void EnsureLoaded() const;

  virtual bool Resolve(TSubclassOf<UFGItemDescriptor> FluidDescriptor, bool bEmpty,
                       FPCAppearanceSpec& OutSpec) const override;

  // Colors only — PaintFinish always loaded fresh via GetFinishClass(Matte).
  const FPCAppearanceSpec& GetNeutral() const {
    return NeutralSpec;
  }

  bool ResolveByKey(FName CatalogKey, FPCAppearanceSpec& OutSpec) const;

  TSubclassOf<UFGFactoryCustomizationDescriptor_PaintFinish>
  GetFinishClass(EPCPaintFinishKind Kind) const;

  // SCIM-safe store seed: never touch cached UClass*.
  static FString GetFinishPath(EPCPaintFinishKind Kind);

  EPCPaintFinishKind FinishKindForKey(FName CatalogKey) const;
  bool IsGasCatalogKey(FName CatalogKey) const;

 private:
  FPCFluidAppearanceCatalog() = default;

  void BuildEntries() const;
  void FillSpecFromEntry(const FPCFluidCatalogEntry& Entry, FPCAppearanceSpec& OutSpec) const;
  void FillNeutralSpec(FPCAppearanceSpec& OutSpec) const;
  static FLinearColor HexRgb(uint8 R, uint8 G, uint8 B);
  static TSubclassOf<UFGItemDescriptor> LoadFluidDesc(const TCHAR* SoftPath, bool bWarnIfMissing);
  static void SeedEntryFromDescriptor(FPCFluidCatalogEntry& Entry,
                                      TSubclassOf<UFGItemDescriptor> Desc,
                                      const struct FPCFluidRosterRow& Row);

  // Lazy-built cache; mutable so const resolvers can trigger the one-time build.
  mutable bool bBuilt = false;
  // PaintFinish left null — Resolve paths call GetFinishClass (TryLoad) each time.
  mutable FPCAppearanceSpec NeutralSpec;
  mutable TMap<FName, FPCFluidCatalogEntry> ByStem;
  mutable TMap<FString, FName> SoftPathToStem;
  // Descriptor UClass* -> stem, weak-keyed: skips FSoftClassPath::ToString per
  // resolve (ran per pipe apply). Compact-on-miss keeps dead weaks bounded.
  mutable TMap<TWeakObjectPtr<UClass>, FName> ClassToStem;
};
