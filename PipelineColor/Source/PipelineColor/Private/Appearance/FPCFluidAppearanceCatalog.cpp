// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Appearance/FPCFluidAppearanceCatalog.h"

#include "PipelineColorLog.h"
#include "Swatches/FPCDynamicSwatchRegistry.h"
#include "Swatches/UPCFinishDescs.h"
#include "Swatches/UPCSwatchDescs.h"
#include "UObject/SoftObjectPath.h"

namespace {
const TCHAR* kFinishDefaultPath =
    TEXT("/Game/FactoryGame/Buildable/-Shared/Customization/PaintFinishes/"
         "PaintFinishDesc_Default.PaintFinishDesc_Default_C");
const TCHAR* kFinishMattePath =
    TEXT("/Game/FactoryGame/Buildable/-Shared/Customization/PaintFinishes/"
         "PaintFinishDesc_Matte.PaintFinishDesc_Matte_C");
} // namespace

FPCFluidAppearanceCatalog& FPCFluidAppearanceCatalog::Get() {
  static FPCFluidAppearanceCatalog Instance;
  return Instance;
}

FLinearColor FPCFluidAppearanceCatalog::HexRgb(uint8 R, uint8 G, uint8 B) {
  return FLinearColor::FromSRGBColor(FColor(R, G, B, 255));
}

FLinearColor FPCFluidAppearanceCatalog::MissingMagenta() {
  return HexRgb(0xFF, 0x00, 0xFF);
}

void FPCFluidAppearanceCatalog::EnsureNeutral() const {
  if (bNeutralReady) {
    return;
  }
  UPCFinish_MetallicColor::EnsureIconLoaded();
  NeutralSpec.SwatchDesc = UPCSwatchDesc_Neutral::StaticClass();
  NeutralSpec.PrimaryColor = HexRgb(0x6E, 0x6E, 0x6E);
  NeutralSpec.SecondaryColor = HexRgb(0x2A, 0x2A, 0x2A);
  NeutralSpec.PaintFinish = nullptr;
  NeutralSpec.CatalogKey = FName(TEXT("Neutral"));
  NeutralSpec.bOverrideRoughness = false;
  bNeutralReady = true;
}

void FPCFluidAppearanceCatalog::EnsureLoaded() const {
  EnsureNeutral();
}

void FPCFluidAppearanceCatalog::Invalidate() const {
  FPCDynamicSwatchRegistry::InvalidateColors();
}

FString FPCFluidAppearanceCatalog::GetFinishPath(EPCPaintFinishKind Kind) {
  if (Kind == EPCPaintFinishKind::MetallicColor) {
    if (UClass* Cls = UPCFinish_MetallicColor::StaticClass()) {
      return FSoftClassPath(Cls).ToString();
    }
    return FString();
  }
  if (Kind == EPCPaintFinishKind::Matte) {
    return FString(kFinishMattePath);
  }
  return FString(kFinishDefaultPath);
}

TSubclassOf<UFGFactoryCustomizationDescriptor_PaintFinish>
FPCFluidAppearanceCatalog::GetFinishClass(EPCPaintFinishKind Kind) const {
  EnsureLoaded();
  if (Kind == EPCPaintFinishKind::MetallicColor) {
    return UPCFinish_MetallicColor::StaticClass();
  }

  static TWeakObjectPtr<UClass> CachedMatte;
  static TWeakObjectPtr<UClass> CachedDefault;
  TWeakObjectPtr<UClass>& Slot = (Kind == EPCPaintFinishKind::Matte) ? CachedMatte : CachedDefault;
  if (UClass* Cached = Slot.Get()) {
    return Cached;
  }

  const FString Path = GetFinishPath(Kind);
  UClass* Loaded =
      FSoftClassPath(Path).TryLoadClass<UFGFactoryCustomizationDescriptor_PaintFinish>();
  if (!Loaded) {
    UE_LOG(LogPipelineColor, Warning, TEXT("%s catalog: failed load finish %s"),
           PIPELINECOLOR_LOG_PREFIX, *Path);
  }
  Slot = Loaded;
  return Loaded;
}

void FPCFluidAppearanceCatalog::FillNeutralSpec(FPCAppearanceSpec& OutSpec) const {
  EnsureNeutral();
  OutSpec = NeutralSpec;
  OutSpec.PaintFinish = GetFinishClass(EPCPaintFinishKind::Matte);
}

void FPCFluidAppearanceCatalog::FillFromDynamic(const FPCDynamicSwatchEntry& Entry,
                                                FPCAppearanceSpec& OutSpec) const {
  OutSpec.SwatchDesc = Entry.SwatchClass;
  OutSpec.CatalogKey = Entry.CatalogKey;
  OutSpec.PrimaryColor = Entry.Primary;
  OutSpec.SecondaryColor = HexRgb(0x2A, 0x2A, 0x2A);
  OutSpec.PaintFinish = GetFinishClass(Entry.Finish);
  OutSpec.bOverrideRoughness = false;
}

EPCPaintFinishKind FPCFluidAppearanceCatalog::FinishKindForKey(FName CatalogKey) const {
  EnsureLoaded();
  if (CatalogKey == FName(TEXT("Neutral"))) {
    return EPCPaintFinishKind::Matte;
  }
  FPCDynamicSwatchEntry Entry;
  if (FPCDynamicSwatchRegistry::TryGetByKey(CatalogKey, Entry)) {
    return Entry.Finish;
  }
  return EPCPaintFinishKind::Default;
}

bool FPCFluidAppearanceCatalog::IsGasCatalogKey(FName CatalogKey) const {
  if (CatalogKey.IsNone()) {
    return false;
  }
  return FinishKindForKey(CatalogKey) == EPCPaintFinishKind::MetallicColor;
}

bool FPCFluidAppearanceCatalog::ResolveByKey(FName CatalogKey, FPCAppearanceSpec& OutSpec) const {
  EnsureLoaded();
  if (CatalogKey.IsNone()) {
    return false;
  }
  if (CatalogKey == FName(TEXT("Neutral"))) {
    FillNeutralSpec(OutSpec);
    return true;
  }
  FPCDynamicSwatchEntry Entry;
  if (FPCDynamicSwatchRegistry::TryGetByKey(CatalogKey, Entry)) {
    FillFromDynamic(Entry, OutSpec);
    return true;
  }
  if (CatalogKey == FName(TEXT("Fallback"))) {
    FillNeutralSpec(OutSpec);
    OutSpec.SwatchDesc = UPCSwatchDesc_Fallback::StaticClass();
    OutSpec.CatalogKey = CatalogKey;
    OutSpec.PrimaryColor = MissingMagenta();
    OutSpec.SecondaryColor = HexRgb(0x2A, 0x2A, 0x2A);
    OutSpec.PaintFinish = GetFinishClass(EPCPaintFinishKind::Default);
    return true;
  }
  return false;
}

bool FPCFluidAppearanceCatalog::Resolve(TSubclassOf<UFGItemDescriptor> FluidDescriptor, bool bEmpty,
                                        FPCAppearanceSpec& OutSpec) const {
  EnsureLoaded();
  if (bEmpty || !FluidDescriptor) {
    FillNeutralSpec(OutSpec);
    return true;
  }

  UClass* Cls = FluidDescriptor.Get();
  if (!Cls) {
    FillNeutralSpec(OutSpec);
    return true;
  }

  FPCDynamicSwatchEntry Entry;
  if (FPCDynamicSwatchRegistry::TryGetByFluidClass(Cls, Entry)) {
    FillFromDynamic(Entry, OutSpec);
    return true;
  }

  // Pipe may carry a fluid Ensure never soft-loaded (GameFeature Desc not in MCR).
  if (FPCDynamicSwatchRegistry::DiscoverClass(Cls) &&
      FPCDynamicSwatchRegistry::TryGetByFluidClass(Cls, Entry)) {
    FillFromDynamic(Entry, OutSpec);
    return true;
  }

  OutSpec.SwatchDesc = UPCSwatchDesc_Fallback::StaticClass();
  OutSpec.CatalogKey = FPCDynamicSwatchRegistry::CatalogKeyFromDescClass(
      Cls, FPCDynamicSwatchRegistry::OwnerModRefFromPackage(Cls));
  if (OutSpec.CatalogKey.IsNone()) {
    OutSpec.CatalogKey = Cls->GetFName();
  }
  OutSpec.PrimaryColor = MissingMagenta();
  OutSpec.SecondaryColor = HexRgb(0x2A, 0x2A, 0x2A);
  const EResourceForm Form = UFGItemDescriptor::GetForm(FluidDescriptor);
  OutSpec.PaintFinish =
      GetFinishClass(Form == EResourceForm::RF_GAS ? EPCPaintFinishKind::MetallicColor
                                                   : EPCPaintFinishKind::Default);
  return true;
}
