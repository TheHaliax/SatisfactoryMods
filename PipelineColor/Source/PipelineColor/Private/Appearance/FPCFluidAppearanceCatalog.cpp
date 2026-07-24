// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Appearance/FPCFluidAppearanceCatalog.h"

#include "Appearance/FPCFluidRoster.h"
#include "PipelineColorLog.h"
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

TSubclassOf<UFGItemDescriptor> FPCFluidAppearanceCatalog::LoadFluidDesc(const TCHAR* SoftPath,
                                                                        bool bWarnIfMissing) {
  const FSoftClassPath Path(SoftPath);
  UClass* Loaded = Path.TryLoadClass<UFGItemDescriptor>();
  if (!Loaded) {
    if (bWarnIfMissing) {
      UE_LOG(LogPipelineColor, Warning, TEXT("%s catalog: failed load fluid %s"),
             PIPELINECOLOR_LOG_PREFIX, SoftPath);
    } else {
      // Mod-section rows are expected to miss when SFP / RP absent — not a fault.
      UE_LOG(LogPipelineColor, Verbose, TEXT("%s catalog: mod fluid absent %s"),
             PIPELINECOLOR_LOG_PREFIX, SoftPath);
    }
  }
  return Loaded;
}

void FPCFluidAppearanceCatalog::EnsureLoaded() const {
  if (bBuilt) {
    return;
  }
  BuildEntries();
  bBuilt = true;
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

  // Two vanilla finish descs, resolved on every spec fill — weak-cache them.
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
  OutSpec = NeutralSpec;
  OutSpec.PaintFinish = GetFinishClass(EPCPaintFinishKind::Matte);
}

void FPCFluidAppearanceCatalog::FillSpecFromEntry(const FPCFluidCatalogEntry& Entry,
                                                  FPCAppearanceSpec& OutSpec) const {
  OutSpec.SwatchDesc = Entry.SwatchClass;
  OutSpec.CatalogKey = Entry.FluidStem;
  OutSpec.PrimaryColor = Entry.Primary;
  OutSpec.SecondaryColor = HexRgb(0x2A, 0x2A, 0x2A);
  OutSpec.PaintFinish = GetFinishClass(Entry.Finish);
  OutSpec.bOverrideRoughness = false;
}

void FPCFluidAppearanceCatalog::SeedEntryFromDescriptor(FPCFluidCatalogEntry& Entry,
                                                        TSubclassOf<UFGItemDescriptor> Desc,
                                                        const FPCFluidRosterRow& Row) {
  const EResourceForm Form = UFGItemDescriptor::GetForm(Desc);
  const bool bGas = Form == EResourceForm::RF_GAS;
  Entry.Finish = bGas ? EPCPaintFinishKind::MetallicColor : EPCPaintFinishKind::Default;

  // Some gases ship without mGasColor (zero black) — fall through to fluid color, then roster.
  const FColor Preferred =
      bGas ? UFGItemDescriptor::GetGasColor(Desc) : UFGItemDescriptor::GetFluidColor(Desc);
  const FColor Other =
      bGas ? UFGItemDescriptor::GetFluidColor(Desc) : UFGItemDescriptor::GetGasColor(Desc);
  auto IsZeroRgb = [](const FColor& C) { return C.R == 0 && C.G == 0 && C.B == 0; };

  FColor Chosen(Row.PrimaryR, Row.PrimaryG, Row.PrimaryB, 255);
  if (!IsZeroRgb(Preferred)) {
    Chosen = Preferred;
  } else if (!IsZeroRgb(Other)) {
    Chosen = Other;
  }
  // Authored alpha is unreliable (SFP ships CoolingWater A=2) — paint is opaque.
  Entry.Primary = HexRgb(Chosen.R, Chosen.G, Chosen.B);

  const bool bColorDrift =
      Chosen.R != Row.PrimaryR || Chosen.G != Row.PrimaryG || Chosen.B != Row.PrimaryB;
  if (bColorDrift || Entry.Finish != Row.Finish) {
    UE_LOG(LogPipelineColor, Log,
           TEXT("%s catalog drift %s: descriptor %d,%d,%d %s vs roster %d,%d,%d %s"),
           PIPELINECOLOR_LOG_PREFIX, *Row.Stem.ToString(), Chosen.R, Chosen.G, Chosen.B,
           bGas ? TEXT("gas") : TEXT("liquid"), Row.PrimaryR, Row.PrimaryG, Row.PrimaryB,
           Row.Finish == EPCPaintFinishKind::MetallicColor ? TEXT("gas") : TEXT("liquid"));
  }
}

void FPCFluidAppearanceCatalog::BuildEntries() const {
  UPCFinish_MetallicColor::EnsureIconLoaded();

  NeutralSpec.SwatchDesc = UPCSwatchDesc_Neutral::StaticClass();
  NeutralSpec.PrimaryColor = HexRgb(0x6E, 0x6E, 0x6E);
  NeutralSpec.SecondaryColor = HexRgb(0x2A, 0x2A, 0x2A);
  NeutralSpec.PaintFinish = nullptr;
  NeutralSpec.CatalogKey = FName(TEXT("Neutral"));
  NeutralSpec.bOverrideRoughness = false;

  ByStem.Reset();
  SoftPathToStem.Reset();
  ClassToStem.Reset();
  int32 LoadedCount = 0;
  int32 ModLoadedCount = 0;
  for (const FPCFluidRosterRow& Row : FPCFluidRoster::FluidRows()) {
    FPCFluidCatalogEntry Entry;
    Entry.FluidStem = Row.Stem;
    Entry.SoftPath = Row.SoftPath;
    Entry.Primary = HexRgb(Row.PrimaryR, Row.PrimaryG, Row.PrimaryB);
    Entry.Finish = Row.Finish;
    Entry.SwatchClass = Row.SwatchClass;

    const bool bDefaultRow = Row.Section == EPCFluidRosterSection::Default;
    if (TSubclassOf<UFGItemDescriptor> Desc = LoadFluidDesc(Row.SoftPath, bDefaultRow)) {
      SeedEntryFromDescriptor(Entry, Desc, Row);
      ++LoadedCount;
      if (!bDefaultRow) {
        ++ModLoadedCount;
      }
    }

    ByStem.Add(Row.Stem, Entry);
    SoftPathToStem.Add(FString(Row.SoftPath), Row.Stem);
  }

  UE_LOG(LogPipelineColor, Log,
         TEXT("%s catalog ready (%d stems, %d fluid classes loaded, %d mod fluids)"),
         PIPELINECOLOR_LOG_PREFIX, ByStem.Num(), LoadedCount, ModLoadedCount);
}

EPCPaintFinishKind FPCFluidAppearanceCatalog::FinishKindForKey(FName CatalogKey) const {
  EnsureLoaded();
  if (CatalogKey == FName(TEXT("Neutral"))) {
    return EPCPaintFinishKind::Matte;
  }
  if (const FPCFluidCatalogEntry* Found = ByStem.Find(CatalogKey)) {
    return Found->Finish;
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

  if (const FPCFluidCatalogEntry* Found = ByStem.Find(CatalogKey)) {
    FillSpecFromEntry(*Found, OutSpec);
    return true;
  }

  if (CatalogKey == FName(TEXT("Fallback"))) {
    FillNeutralSpec(OutSpec);
    OutSpec.SwatchDesc = UPCSwatchDesc_Fallback::StaticClass();
    OutSpec.CatalogKey = CatalogKey;
    OutSpec.PrimaryColor = FLinearColor::FromSRGBColor(FColor::White);
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

  if (const FName* CachedStem = ClassToStem.Find(Cls)) {
    if (const FPCFluidCatalogEntry* Found = ByStem.Find(*CachedStem)) {
      FillSpecFromEntry(*Found, OutSpec);
      return true;
    }
  }

  const FString SoftPath = FSoftClassPath(Cls).ToString();
  if (const FName* Stem = SoftPathToStem.Find(SoftPath)) {
    if (const FPCFluidCatalogEntry* Found = ByStem.Find(*Stem)) {
      // Miss path also sweeps stale weak keys so the cache cannot grow past
      // the live descriptor set.
      for (auto It = ClassToStem.CreateIterator(); It; ++It) {
        if (!It.Key().IsValid()) {
          It.RemoveCurrent();
        }
      }
      ClassToStem.Add(Cls, *Stem);
      FillSpecFromEntry(*Found, OutSpec);
      return true;
    }
  }

  OutSpec.SwatchDesc = UPCSwatchDesc_Fallback::StaticClass();
  OutSpec.CatalogKey = Cls->GetFName();
  OutSpec.PrimaryColor = UFGItemDescriptor::GetFluidColorLinear(FluidDescriptor);
  OutSpec.SecondaryColor = HexRgb(0x2A, 0x2A, 0x2A);
  OutSpec.PaintFinish = GetFinishClass(EPCPaintFinishKind::Default);
  return true;
}
