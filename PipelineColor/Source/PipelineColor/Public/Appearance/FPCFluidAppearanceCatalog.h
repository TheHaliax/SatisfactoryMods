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
  void Invalidate() const;

  virtual bool Resolve(TSubclassOf<UFGItemDescriptor> FluidDescriptor, bool bEmpty,
                       FPCAppearanceSpec& OutSpec) const override;

  const FPCAppearanceSpec& GetNeutral() const {
    return NeutralSpec;
  }

  bool ResolveByKey(FName CatalogKey, FPCAppearanceSpec& OutSpec) const;

  TSubclassOf<UFGFactoryCustomizationDescriptor_PaintFinish>
  GetFinishClass(EPCPaintFinishKind Kind) const;

  // SoftClassPath in SaveGame breaks SCIM — path strings only.
  static FString GetFinishPath(EPCPaintFinishKind Kind);

  EPCPaintFinishKind FinishKindForKey(FName CatalogKey) const;
  bool IsGasCatalogKey(FName CatalogKey) const;

 private:
  FPCFluidAppearanceCatalog() = default;

  void BuildEntries() const;
  void FillSpecFromEntry(const FPCFluidCatalogEntry& Entry, FPCAppearanceSpec& OutSpec) const;
  void FillNeutralSpec(FPCAppearanceSpec& OutSpec) const;
  static FLinearColor HexRgb(uint8 R, uint8 G, uint8 B);
  static FLinearColor MissingMagenta();
  static TSubclassOf<UFGItemDescriptor> LoadFluidDesc(const TCHAR* SoftPath, bool bWarnIfMissing);
  static void SeedEntryFromDescriptor(FPCFluidCatalogEntry& Entry,
                                      TSubclassOf<UFGItemDescriptor> Desc,
                                      const struct FPCFluidRosterRow& Row);

  mutable bool bBuilt = false;
  mutable FPCAppearanceSpec NeutralSpec;
  mutable TMap<FName, FPCFluidCatalogEntry> ByStem;
  mutable TMap<FString, FName> SoftPathToStem;
  mutable TMap<FString, FName> SoftPathNormToStem;
  mutable TMap<TWeakObjectPtr<UClass>, FName> ClassToStem;
};
