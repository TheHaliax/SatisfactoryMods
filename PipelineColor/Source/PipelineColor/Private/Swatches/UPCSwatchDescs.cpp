// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Swatches/UPCSwatchDescs.h"

#define LOCTEXT_NAMESPACE "PipelineColor"

namespace {
void InitSwatch(UPCSwatchDescBase* Self, const FText& Name, FName Key) {
  if (!Self) {
    return;
  }
  Self->mUseDisplayNameAndDescription = true;
  Self->mDisplayName = Name;
  Self->mDescription = NSLOCTEXT("PipelineColor", "SwatchDesc", "PipelineColor fluid swatch");
  Self->ID = INDEX_CUSTOM_COLOR_SLOT;
  Self->CatalogKey = Key;
  Self->mValidBuildables.Reset();
}
} // namespace

UPCSwatchDescBase::UPCSwatchDescBase() {
  mUseDisplayNameAndDescription = true;
  ID = INDEX_CUSTOM_COLOR_SLOT;
}

FName UPCSwatchDescBase::GetCatalogKey(
    TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Swatch) {
  if (!Swatch) {
    return NAME_None;
  }
  if (const UPCSwatchDescBase* CDO = Cast<UPCSwatchDescBase>(Swatch->GetDefaultObject())) {
    return CDO->CatalogKey;
  }
  return NAME_None;
}

UPCSwatchDesc_Neutral::UPCSwatchDesc_Neutral() {
  InitSwatch(this, NSLOCTEXT("PipelineColor", "SwatchNeutral", "PC Empty Pipe"),
             FName(TEXT("Neutral")));
}

UPCSwatchDesc_Fallback::UPCSwatchDesc_Fallback() {
  InitSwatch(this, NSLOCTEXT("PipelineColor", "SwatchFallback", "PC ERR0R"),
             FName(TEXT("Fallback")));
}

#undef LOCTEXT_NAMESPACE
