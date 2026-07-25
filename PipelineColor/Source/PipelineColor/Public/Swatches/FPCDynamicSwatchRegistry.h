// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "Appearance/FPCFluidAppearanceCatalog.h"
#include "CoreMinimal.h"
#include "FGCustomizationRecipe.h"
#include "FGFactoryColoringTypes.h"
#include "Resources/FGItemDescriptor.h"
#include "Templates/SubclassOf.h"

class UPipelineColorRootInstanceModule;
class UWorld;

struct FPCDynamicSwatchEntry {
  FName CatalogKey = NAME_None;
  TSubclassOf<UFGItemDescriptor> FluidDesc;
  TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> SwatchClass;
  TSubclassOf<UFGCustomizationRecipe> RecipeClass;
  FName OwnerModRef = NAME_None;
  FString OwnerFriendlyName;
  EResourceForm Form = EResourceForm::RF_INVALID;
  FLinearColor Primary = FLinearColor(0.43f, 0.43f, 0.43f, 1.f);
  EPCPaintFinishKind Finish = EPCPaintFinishKind::Default;
};

namespace FPCDynamicSwatchRegistry {
void Ensure(UWorld* World, UPipelineColorRootInstanceModule* Root, bool bForceRescan = false);
bool DiscoverClass(UClass* DescClass);
void InvalidateColors();
void Reset();

const TArray<FPCDynamicSwatchEntry>& Entries();
bool TryGetByKey(FName CatalogKey, FPCDynamicSwatchEntry& Out);
bool TryGetByFluidClass(UClass* FluidClass, FPCDynamicSwatchEntry& Out);

void AppendSwatchClasses(TArray<TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch>>& Out);
void AppendRecipeClasses(TArray<TSubclassOf<UFGCustomizationRecipe>>& Out);

FName StemFromDescClass(UClass* DescClass);
FName CatalogKeyFromDescClass(UClass* DescClass, FName OwnerModRef);
FName OwnerModRefFromPackage(UClass* Cls);
FLinearColor PrimaryFromDescriptor(TSubclassOf<UFGItemDescriptor> Desc, FName CatalogKey);
} // namespace FPCDynamicSwatchRegistry
