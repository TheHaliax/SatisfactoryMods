// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Swatches/FPCDynamicSwatchRegistry.h"

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Config/FPCPipelineColorModConfig.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "FGRecipe.h"
#include "ItemAmount.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "ModLoading/ModLoadingLibrary.h"
#include "PipelineColorLog.h"
#include "PipelineColorRootInstanceModule.h"
#include "Reflection/ClassGenerator.h"
#include "Registry/ModContentRegistry.h"
#include "Store/APCSwatchStoreSubsystem.h"
#include "Swatches/UPCSwatchDescs.h"
#include "Swatches/UPCSwatchRecipes.h"
#include "Swatches/UPCSwatchSubCategory.h"
#include "UObject/SoftObjectPath.h"

namespace {
TArray<FPCDynamicSwatchEntry> GEntries;
TMap<FName, int32> GByKey;
TMap<TWeakObjectPtr<UClass>, int32> GByClass;
TWeakObjectPtr<UWorld> GEnsuredWorld;
bool GColorsDirty = false;
TSet<FName> GUsedClassTokens;

bool IsZeroRgb(const FColor& C) {
  return C.R == 0 && C.G == 0 && C.B == 0;
}

FString SanitizeClassToken(FName Key) {
  FString S = Key.ToString();
  for (TCHAR& C : S) {
    if (!FChar::IsAlnum(C)) {
      C = TEXT('_');
    }
  }
  if (S.IsEmpty()) {
    S = TEXT("Unknown");
  }
  return S;
}

FString UniqueClassToken(FName CatalogKey) {
  FString Token = SanitizeClassToken(CatalogKey);
  if (!GUsedClassTokens.Contains(FName(*Token))) {
    GUsedClassTokens.Add(FName(*Token));
    return Token;
  }
  const uint32 Hash = GetTypeHash(CatalogKey);
  FString Hashed = FString::Printf(TEXT("%s_%08X"), *Token, Hash);
  GUsedClassTokens.Add(FName(*Hashed));
  return Hashed;
}

UClass* FindOrGenerate(const TCHAR* PackageName, const FString& ClassName, UClass* Parent) {
  if (!Parent || ClassName.IsEmpty()) {
    return nullptr;
  }
  const FString FullPath = FString::Printf(TEXT("%s.%s"), PackageName, *ClassName);
  if (UClass* Existing = FindObject<UClass>(nullptr, *FullPath)) {
    return Existing;
  }
  return FClassGenerator::GenerateSimpleClass(PackageName, *ClassName, Parent);
}

void BakeSwatchCdo(UClass* Generated, FName CatalogKey, const FText& DisplayName) {
  UPCSwatchDescBase* CDO =
      Generated ? Cast<UPCSwatchDescBase>(Generated->GetDefaultObject()) : nullptr;
  if (!CDO) {
    return;
  }
  CDO->mUseDisplayNameAndDescription = true;
  CDO->mDisplayName = DisplayName;
  CDO->mDescription = FText::FromString(TEXT("PipelineColor fluid swatch"));
  CDO->ID = INDEX_CUSTOM_COLOR_SLOT;
  CDO->CatalogKey = CatalogKey;
  CDO->mValidBuildables.Reset();
}

void BakeRecipeCdo(UClass* Generated, TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Desc,
                   const FString& DisplayName) {
  UPCSwatchRecipeBase* CDO =
      Generated ? Cast<UPCSwatchRecipeBase>(Generated->GetDefaultObject()) : nullptr;
  if (!CDO) {
    return;
  }
  CDO->InitRecipe(Desc, *DisplayName);
}

FText DisplayNameForFluid(TSubclassOf<UFGItemDescriptor> Desc, FName CatalogKey) {
  if (Desc) {
    const FText ItemName = UFGItemDescriptor::GetItemName(Desc);
    if (!ItemName.IsEmpty()) {
      return ItemName;
    }
  }
  return FText::FromName(CatalogKey);
}

void RebakeEntryLabels(const FPCDynamicSwatchEntry& Entry) {
  UClass* SwatchCls = Entry.SwatchClass.Get();
  if (!SwatchCls || Entry.CatalogKey.IsNone()) {
    return;
  }
  const FText DisplayName = DisplayNameForFluid(Entry.FluidDesc, Entry.CatalogKey);
  BakeSwatchCdo(SwatchCls, Entry.CatalogKey, DisplayName);
  if (UClass* RecipeCls = Entry.RecipeClass.Get()) {
    BakeRecipeCdo(RecipeCls, Entry.SwatchClass, DisplayName.ToString());
  }
}

void RefreshAllLabels() {
  for (const FPCDynamicSwatchEntry& Entry : GEntries) {
    RebakeEntryLabels(Entry);
  }
}

void RefreshPrimaryColors() {
  for (FPCDynamicSwatchEntry& Entry : GEntries) {
    if (!Entry.FluidDesc) {
      continue;
    }
    Entry.Primary =
        FPCDynamicSwatchRegistry::PrimaryFromDescriptor(Entry.FluidDesc, Entry.CatalogKey);
  }
  GColorsDirty = false;
}

void RebuildMaps() {
  GByKey.Reset();
  GByClass.Reset();
  for (int32 i = 0; i < GEntries.Num(); ++i) {
    const FPCDynamicSwatchEntry& Entry = GEntries[i];
    if (!Entry.CatalogKey.IsNone()) {
      GByKey.Add(Entry.CatalogKey, i);
    }
    if (UClass* Cls = Entry.FluidDesc.Get()) {
      GByClass.Add(Cls, i);
    }
  }
}

FString ResolveOwnerFriendlyNameFromModRef(UWorld* World, FName OwnedByModReference,
                                           bool bBuiltIn) {
  if (bBuiltIn || OwnedByModReference.IsNone() ||
      OwnedByModReference == FName(TEXT("FactoryGame"))) {
    return TEXT("Default");
  }
  UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
  UModLoadingLibrary* Mods = GI ? GI->GetSubsystem<UModLoadingLibrary>() : nullptr;
  if (Mods) {
    FModInfo Info;
    if (Mods->GetLoadedModInfo(OwnedByModReference.ToString(), Info) &&
        !Info.FriendlyName.IsEmpty()) {
      return Info.FriendlyName;
    }
  }
  return OwnedByModReference.ToString();
}

bool PreferIncomingFluidClass(UClass* Incoming, UClass* Existing) {
  if (!Incoming) {
    return false;
  }
  if (!Existing) {
    return true;
  }
  const bool bInBad =
      Incoming->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists);
  const bool bExBad =
      Existing->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists);
  if (bInBad != bExBad) {
    return !bInBad;
  }
  return false;
}

bool TryAddFluidDesc(UWorld* World, UPipelineColorRootInstanceModule* Root, UClass* Cls,
                     FName OwnedByModReference, bool bBuiltIn, int32& OutSkippedDup,
                     int32& OutSkippedBad) {
  if (!Cls || !Cls->IsChildOf(UFGItemDescriptor::StaticClass())) {
    ++OutSkippedBad;
    return false;
  }
  TSubclassOf<UFGItemDescriptor> Desc = Cls;
  const EResourceForm Form = UFGItemDescriptor::GetForm(Desc);
  if (Form != EResourceForm::RF_LIQUID && Form != EResourceForm::RF_GAS) {
    return false;
  }

  const FName Owner =
      bBuiltIn || OwnedByModReference.IsNone() || OwnedByModReference == FName(TEXT("FactoryGame"))
          ? FName(TEXT("FactoryGame"))
          : OwnedByModReference;
  const FName CatalogKey = FPCDynamicSwatchRegistry::CatalogKeyFromDescClass(Cls, Owner);
  if (CatalogKey.IsNone()) {
    ++OutSkippedBad;
    return false;
  }
  if (const int32* ExistingIdx = GByKey.Find(CatalogKey)) {
    FPCDynamicSwatchEntry& Existing = GEntries[*ExistingIdx];
    if (PreferIncomingFluidClass(Cls, Existing.FluidDesc.Get())) {
      Existing.FluidDesc = Desc;
      Existing.Form = Form;
      Existing.Finish = Form == EResourceForm::RF_GAS ? EPCPaintFinishKind::MetallicColor
                                                      : EPCPaintFinishKind::Default;
      Existing.Primary = FPCDynamicSwatchRegistry::PrimaryFromDescriptor(Desc, CatalogKey);
      RebuildMaps();
    }
    RebakeEntryLabels(Existing);
    ++OutSkippedDup;
    return false;
  }

  const FString Token = UniqueClassToken(CatalogKey);
  UClass* SwatchGen =
      FindOrGenerate(TEXT("/PipelineColor"), FString::Printf(TEXT("PCSwatch_%s"), *Token),
                     UPCSwatchDescBase::StaticClass());
  UClass* RecipeGen =
      FindOrGenerate(TEXT("/PipelineColor"), FString::Printf(TEXT("PCRecipe_%s"), *Token),
                     UPCSwatchRecipeBase::StaticClass());
  if (!SwatchGen || !RecipeGen) {
    UE_LOG(LogPipelineColor, Error, TEXT("%s ClassGen failed for %s"), PIPELINECOLOR_LOG_PREFIX,
           *CatalogKey.ToString());
    ++OutSkippedBad;
    return false;
  }

  const FText DisplayName = DisplayNameForFluid(Desc, CatalogKey);
  BakeSwatchCdo(SwatchGen, CatalogKey, DisplayName);
  BakeRecipeCdo(RecipeGen, SwatchGen, DisplayName.ToString());

  if (IsValid(Root)) {
    Root->DynamicSwatchClasses.AddUnique(SwatchGen);
    Root->DynamicRecipeClasses.AddUnique(RecipeGen);
  }

  FPCDynamicSwatchEntry Entry;
  Entry.CatalogKey = CatalogKey;
  Entry.FluidDesc = Desc;
  Entry.SwatchClass = SwatchGen;
  Entry.RecipeClass = RecipeGen;
  Entry.OwnerModRef = Owner;
  Entry.OwnerFriendlyName =
      ResolveOwnerFriendlyNameFromModRef(World, Owner, Owner == FName(TEXT("FactoryGame")));
  Entry.Form = Form;
  Entry.Finish = Form == EResourceForm::RF_GAS ? EPCPaintFinishKind::MetallicColor
                                               : EPCPaintFinishKind::Default;
  Entry.Primary = FPCDynamicSwatchRegistry::PrimaryFromDescriptor(Desc, CatalogKey);

  GByKey.Add(CatalogKey, GEntries.Num());
  GByClass.Add(Cls, GEntries.Num());
  GEntries.Add(Entry);
  return true;
}

void LogDiscoverySummary(int32 McrScanned, int32 RecipeScanned, int32 AssetScanned,
                         int32 SkippedDup, int32 SkippedBad) {
  TMap<FString, int32> ByOwner;
  for (const FPCDynamicSwatchEntry& Entry : GEntries) {
    const FString Owner =
        Entry.OwnerFriendlyName.IsEmpty() ? TEXT("(unknown)") : Entry.OwnerFriendlyName;
    ByOwner.FindOrAdd(Owner)++;
  }

  TArray<FString> OwnerLines;
  for (const TPair<FString, int32>& Pair : ByOwner) {
    OwnerLines.Add(FString::Printf(TEXT("%s=%d"), *Pair.Key, Pair.Value));
  }
  OwnerLines.Sort();

  UE_LOG(LogPipelineColor, Log,
         TEXT("%s dynamic swatches ready (%d fluids; mcr=%d recipeProducts=%d assets=%d "
              "skipDup=%d skipBad=%d)"),
         PIPELINECOLOR_LOG_PREFIX, GEntries.Num(), McrScanned, RecipeScanned, AssetScanned,
         SkippedDup, SkippedBad);
  if (OwnerLines.Num() > 0) {
    UE_LOG(LogPipelineColor, Log, TEXT("%s dynamic swatches by owner: %s"),
           PIPELINECOLOR_LOG_PREFIX, *FString::Join(OwnerLines, TEXT(", ")));
  }
  if (GEntries.Num() == 0) {
    UE_LOG(LogPipelineColor, Warning, TEXT("%s dynamic swatches empty — MCR fluids not ready yet"),
           PIPELINECOLOR_LOG_PREFIX);
  }
}

UWorld* FindGameplayWorld() {
  if (!GEngine) {
    return nullptr;
  }
  for (const FWorldContext& Ctx : GEngine->GetWorldContexts()) {
    UWorld* World = Ctx.World();
    if (IsValid(World) && World->IsGameWorld()) {
      return World;
    }
  }
  return nullptr;
}

void CollectAssetPackageRoots(UWorld* World, TArray<FName>& OutRoots) {
  TArray<FString> RootPaths;
  FPackageName::QueryRootContentPaths(RootPaths);
  for (const FString& Path : RootPaths) {
    if (Path.IsEmpty() || Path.StartsWith(TEXT("/Engine")) || Path.StartsWith(TEXT("/Script")) ||
        Path.StartsWith(TEXT("/Paper")) || Path.StartsWith(TEXT("/Niagara")) ||
        Path.StartsWith(TEXT("/Media")) || Path.StartsWith(TEXT("/WebM"))) {
      continue;
    }
    OutRoots.AddUnique(FName(*Path));
  }

  UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
  UModLoadingLibrary* Mods = GI ? GI->GetSubsystem<UModLoadingLibrary>() : nullptr;
  if (Mods) {
    for (const FModInfo& Info : Mods->GetLoadedMods()) {
      if (!Info.Name.IsEmpty()) {
        OutRoots.AddUnique(FName(*FString::Printf(TEXT("/%s"), *Info.Name)));
      }
    }
  }
}

UClass* TryLoadDescClassFromAsset(const FAssetData& Asset) {
  const FString AssetName = Asset.AssetName.ToString();
  if (!AssetName.StartsWith(TEXT("Desc_"))) {
    return nullptr;
  }

  FString ObjectPath = Asset.GetObjectPathString();
  if (ObjectPath.IsEmpty()) {
    return nullptr;
  }
  FString ClassPath = ObjectPath;
  if (!ClassPath.EndsWith(TEXT("_C"))) {
    ClassPath += TEXT("_C");
  }

  if (UClass* Cls = LoadClass<UFGItemDescriptor>(nullptr, *ClassPath, nullptr,
                                                 LOAD_NoWarn | LOAD_Quiet, nullptr)) {
    return Cls;
  }
  if (ClassPath != ObjectPath) {
    if (UClass* Cls = LoadClass<UFGItemDescriptor>(nullptr, *ObjectPath, nullptr,
                                                   LOAD_NoWarn | LOAD_Quiet, nullptr)) {
      return Cls;
    }
  }
  return nullptr;
}
} // namespace

FName FPCDynamicSwatchRegistry::StemFromDescClass(UClass* DescClass) {
  if (!DescClass) {
    return NAME_None;
  }
  FString Name = DescClass->GetName();
  if (Name.EndsWith(TEXT("_C"))) {
    Name.LeftChopInline(2);
  }
  if (Name.StartsWith(TEXT("Desc_"))) {
    Name.RightChopInline(5);
  }
  return FName(*Name);
}

FName FPCDynamicSwatchRegistry::OwnerModRefFromPackage(UClass* Cls) {
  if (!Cls) {
    return FName(TEXT("FactoryGame"));
  }
  const FString Pkg = Cls->GetOutermost()->GetName();
  if (Pkg.StartsWith(TEXT("/Game/FactoryGame")) || Pkg.StartsWith(TEXT("/Game/"))) {
    return FName(TEXT("FactoryGame"));
  }
  if (Pkg.StartsWith(TEXT("/"))) {
    FString Root;
    FString Rest;
    if (Pkg.Mid(1).Split(TEXT("/"), &Root, &Rest) && !Root.IsEmpty()) {
      if (Root == TEXT("Game") || Root == TEXT("Engine") || Root == TEXT("Script")) {
        return FName(TEXT("FactoryGame"));
      }
      return FName(*Root);
    }
  }
  return FName(TEXT("FactoryGame"));
}

FName FPCDynamicSwatchRegistry::CatalogKeyFromDescClass(UClass* DescClass, FName OwnerModRef) {
  const FName Stem = StemFromDescClass(DescClass);
  if (Stem.IsNone()) {
    return NAME_None;
  }
  FName Owner = OwnerModRef;
  if (Owner.IsNone() || Owner == FName(TEXT("FactoryGame"))) {
    Owner = FName(TEXT("FactoryGame"));
  }
  return FName(*FString::Printf(TEXT("%s_%s"), *Owner.ToString(), *Stem.ToString()));
}

FLinearColor FPCDynamicSwatchRegistry::PrimaryFromDescriptor(TSubclassOf<UFGItemDescriptor> Desc,
                                                             FName CatalogKey) {
  if (!Desc) {
    return FLinearColor(0.43f, 0.43f, 0.43f, 1.f);
  }
  const bool bWantGas =
      FPCPipelineColorModConfig::GetColorSourceForKey(CatalogKey) == EPCColorSource::Gas;
  const FColor Preferred =
      bWantGas ? UFGItemDescriptor::GetGasColor(Desc) : UFGItemDescriptor::GetFluidColor(Desc);
  const FColor Other =
      bWantGas ? UFGItemDescriptor::GetFluidColor(Desc) : UFGItemDescriptor::GetGasColor(Desc);
  if (!IsZeroRgb(Preferred)) {
    return FLinearColor::FromSRGBColor(FColor(Preferred.R, Preferred.G, Preferred.B, 255));
  }
  if (!IsZeroRgb(Other)) {
    return FLinearColor::FromSRGBColor(FColor(Other.R, Other.G, Other.B, 255));
  }
  return FLinearColor(0.43f, 0.43f, 0.43f, 1.f);
}

void FPCDynamicSwatchRegistry::Reset() {
  GEntries.Reset();
  GByKey.Reset();
  GByClass.Reset();
  GEnsuredWorld.Reset();
  GColorsDirty = false;
  GUsedClassTokens.Reset();
}

void FPCDynamicSwatchRegistry::InvalidateColors() {
  GColorsDirty = true;
}

const TArray<FPCDynamicSwatchEntry>& FPCDynamicSwatchRegistry::Entries() {
  if (GColorsDirty) {
    RefreshPrimaryColors();
  }
  return GEntries;
}

bool FPCDynamicSwatchRegistry::TryGetByKey(FName CatalogKey, FPCDynamicSwatchEntry& Out) {
  if (GColorsDirty) {
    RefreshPrimaryColors();
  }
  if (const int32* Idx = GByKey.Find(CatalogKey)) {
    Out = GEntries[*Idx];
    return true;
  }
  return false;
}

bool FPCDynamicSwatchRegistry::TryGetByFluidClass(UClass* FluidClass, FPCDynamicSwatchEntry& Out) {
  if (GColorsDirty) {
    RefreshPrimaryColors();
  }
  if (!FluidClass) {
    return false;
  }
  if (const int32* Idx = GByClass.Find(FluidClass)) {
    Out = GEntries[*Idx];
    return true;
  }
  return false;
}

void FPCDynamicSwatchRegistry::AppendSwatchClasses(
    TArray<TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch>>& Out) {
  Out.Add(UPCSwatchDesc_Neutral::StaticClass());
  for (const FPCDynamicSwatchEntry& Entry : Entries()) {
    if (Entry.SwatchClass) {
      Out.Add(Entry.SwatchClass);
    }
  }
  Out.Add(UPCSwatchDesc_Fallback::StaticClass());
}

void FPCDynamicSwatchRegistry::AppendRecipeClasses(
    TArray<TSubclassOf<UFGCustomizationRecipe>>& Out) {
  Out.Add(UPCSwatchRecipe_Neutral::StaticClass());
  for (const FPCDynamicSwatchEntry& Entry : Entries()) {
    if (Entry.RecipeClass) {
      Out.Add(Entry.RecipeClass);
    }
  }
  Out.Add(UPCSwatchRecipe_Fallback::StaticClass());
}

void FPCDynamicSwatchRegistry::Ensure(UWorld* World, UPipelineColorRootInstanceModule* Root,
                                      bool bForceRescan) {
  if (!IsValid(World)) {
    return;
  }
  if (!bForceRescan && GEnsuredWorld.Get() == World && GEntries.Num() > 0) {
    RefreshAllLabels();
    if (GColorsDirty) {
      RefreshPrimaryColors();
    }
    return;
  }

  GEntries.Reset();
  GByKey.Reset();
  GByClass.Reset();
  GUsedClassTokens.Reset();
  GEnsuredWorld = World;
  GColorsDirty = false;

  if (IsValid(Root)) {
    UPipelineColorRootInstanceModule::GetOrCreatePipelineColorCategory(Root);
    UPipelineColorRootInstanceModule::GetOrCreatePipelineColorSubCategory(Root);
  }

  int32 McrScanned = 0;
  int32 RecipeScanned = 0;
  int32 AssetScanned = 0;
  int32 SkippedDup = 0;
  int32 SkippedBad = 0;

  auto AddWithOwner = [&](UClass* Cls, FName OwnerOverride, bool bBuiltIn) {
    FName Owner = OwnerOverride;
    if (Owner.IsNone()) {
      Owner = OwnerModRefFromPackage(Cls);
    }
    const bool bDefault = bBuiltIn || Owner.IsNone() || Owner == FName(TEXT("FactoryGame"));
    TryAddFluidDesc(World, Root, Cls, bDefault ? FName(TEXT("FactoryGame")) : Owner, bDefault,
                    SkippedDup, SkippedBad);
  };

  UModContentRegistry* Registry = UModContentRegistry::Get(World);
  if (Registry) {
    const TArray<FGameObjectRegistration> Loaded = Registry->GetLoadedItemDescriptors();
    for (const FGameObjectRegistration& Reg : Loaded) {
      if (Reg.HasAnyFlags(EGameObjectRegistrationFlags::Removed |
                          EGameObjectRegistrationFlags::Unregistered)) {
        continue;
      }
      UClass* Cls = Cast<UClass>(Reg.RegisteredObject.Get());
      if (!Cls) {
        continue;
      }
      ++McrScanned;
      AddWithOwner(Cls, Reg.OwnedByModReference,
                   Reg.HasAnyFlags(EGameObjectRegistrationFlags::BuiltIn));
    }

    for (const FGameObjectRegistration& RecipeReg : Registry->GetRegisteredRecipes()) {
      if (RecipeReg.HasAnyFlags(EGameObjectRegistrationFlags::Removed |
                                EGameObjectRegistrationFlags::Unregistered)) {
        continue;
      }
      UClass* RecipeCls = Cast<UClass>(RecipeReg.RegisteredObject.Get());
      if (!RecipeCls || !RecipeCls->IsChildOf(UFGRecipe::StaticClass())) {
        continue;
      }
      const TArray<FItemAmount> Products = UFGRecipe::GetProducts(RecipeCls);
      for (const FItemAmount& Product : Products) {
        UClass* Cls = Product.ItemClass.Get();
        if (!Cls) {
          continue;
        }
        ++RecipeScanned;
        FName Owner = RecipeReg.OwnedByModReference;
        const FGameObjectRegistration ItemInfo = Registry->GetItemDescriptorInfo(Cls);
        if (!ItemInfo.OwnedByModReference.IsNone()) {
          Owner = ItemInfo.OwnedByModReference;
        }
        AddWithOwner(Cls, Owner, ItemInfo.HasAnyFlags(EGameObjectRegistrationFlags::BuiltIn));
      }
    }
  } else {
    UE_LOG(LogPipelineColor, Warning, TEXT("%s dynamic swatches: ModContentRegistry missing"),
           PIPELINECOLOR_LOG_PREFIX);
  }

  {
    FAssetRegistryModule& Arm =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    IAssetRegistry& AssetRegistry = Arm.Get();
    TArray<FName> Roots;
    CollectAssetPackageRoots(World, Roots);
    TArray<FAssetData> Assets;
    for (const FName PackageRoot : Roots) {
      FARFilter Filter;
      Filter.PackagePaths.Add(PackageRoot);
      Filter.bRecursivePaths = true;
      AssetRegistry.GetAssets(Filter, Assets);
    }
    for (const FAssetData& Asset : Assets) {
      UClass* Cls = TryLoadDescClassFromAsset(Asset);
      if (!Cls) {
        continue;
      }
      ++AssetScanned;
      AddWithOwner(Cls, NAME_None, false);
    }
  }

  RebuildMaps();
  LogDiscoverySummary(McrScanned, RecipeScanned, AssetScanned, SkippedDup, SkippedBad);
}

bool FPCDynamicSwatchRegistry::DiscoverClass(UClass* DescClass) {
  if (!DescClass || !DescClass->IsChildOf(UFGItemDescriptor::StaticClass())) {
    return false;
  }
  if (const int32* ByClassIdx = GByClass.Find(DescClass)) {
    RebakeEntryLabels(GEntries[*ByClassIdx]);
    return true;
  }

  UWorld* World = GEnsuredWorld.Get();
  if (!IsValid(World)) {
    World = FindGameplayWorld();
  }
  UPipelineColorRootInstanceModule* Root =
      IsValid(World) ? UPipelineColorRootInstanceModule::Find(World) : nullptr;
  int32 SkippedDup = 0;
  int32 SkippedBad = 0;
  const FName Owner = OwnerModRefFromPackage(DescClass);
  const bool bDefault = Owner.IsNone() || Owner == FName(TEXT("FactoryGame"));
  if (!TryAddFluidDesc(World, Root, DescClass, bDefault ? FName(TEXT("FactoryGame")) : Owner,
                       bDefault, SkippedDup, SkippedBad)) {
    if (const int32* ByClassIdx = GByClass.Find(DescClass)) {
      return true;
    }
    const FName Key = CatalogKeyFromDescClass(DescClass, Owner);
    return !Key.IsNone() && GByKey.Contains(Key);
  }
  RebuildMaps();
  UE_LOG(LogPipelineColor, Log, TEXT("%s lazy-discovered fluid %s"), PIPELINECOLOR_LOG_PREFIX,
         *CatalogKeyFromDescClass(DescClass, Owner).ToString());
  return true;
}
