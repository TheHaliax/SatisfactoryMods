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

TSubclassOf<UFGItemDescriptor> FPCFluidAppearanceCatalog::LoadFluidDesc(const TCHAR* SoftPath) {
  const FSoftClassPath Path(SoftPath);
  UClass* Loaded = Path.TryLoadClass<UFGItemDescriptor>();
  if (!Loaded) {
    UE_LOG(LogPipelineColor, Warning, TEXT("%s catalog: failed load fluid %s"),
           PIPELINECOLOR_LOG_PREFIX, SoftPath);
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
  const FString Path = GetFinishPath(Kind);
  UClass* Loaded =
      FSoftClassPath(Path).TryLoadClass<UFGFactoryCustomizationDescriptor_PaintFinish>();
  if (!Loaded) {
    UE_LOG(LogPipelineColor, Warning, TEXT("%s catalog: failed load finish %s"),
           PIPELINECOLOR_LOG_PREFIX, *Path);
  }
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
  int32 LoadedCount = 0;
  for (const FPCFluidRosterRow& Row : FPCFluidRoster::FluidRows()) {
    FPCFluidCatalogEntry Entry;
    Entry.FluidStem = Row.Stem;
    Entry.SoftPath = Row.SoftPath;
    Entry.Primary = HexRgb(Row.PrimaryR, Row.PrimaryG, Row.PrimaryB);
    Entry.Finish = Row.Finish;
    Entry.SwatchClass = Row.SwatchClass;
    ByStem.Add(Row.Stem, Entry);
    SoftPathToStem.Add(FString(Row.SoftPath), Row.Stem);

    if (LoadFluidDesc(Row.SoftPath)) {
      ++LoadedCount;
    }
  }

  UE_LOG(LogPipelineColor, Log, TEXT("%s catalog ready (%d stems, %d fluid classes loaded)"),
         PIPELINECOLOR_LOG_PREFIX, ByStem.Num(), LoadedCount);
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

  const FString SoftPath = FSoftClassPath(Cls).ToString();
  if (const FName* Stem = SoftPathToStem.Find(SoftPath)) {
    if (const FPCFluidCatalogEntry* Found = ByStem.Find(*Stem)) {
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
