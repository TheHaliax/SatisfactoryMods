// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "FGSaveInterface.h"
#include "GameFramework/Info.h"
#include "Store/FPCSwatchEntry.h"
#include "APCSwatchStoreSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FPCSwatchEntryChanged, FName /*Key*/);

UCLASS()
class PIPELINECOLOR_API APCSwatchStoreSubsystem : public AInfo, public IFGSaveInterface {
  GENERATED_BODY()

 public:
  APCSwatchStoreSubsystem();

  virtual void GetLifetimeReplicatedProps(
      TArray<FLifetimeProperty>& OutLifetimeProps) const override;

  static APCSwatchStoreSubsystem* Find(UWorld* World);
  static APCSwatchStoreSubsystem* GetOrCreate(UWorld* World);

  virtual void PreSaveGame_Implementation(int32 SaveVersion, int32 GameVersion) override {
  }
  virtual void PostSaveGame_Implementation(int32 SaveVersion, int32 GameVersion) override {
  }
  virtual void PostLoadGame_Implementation(int32 SaveVersion, int32 GameVersion) override;
  virtual void GatherDependencies_Implementation(TArray<UObject*>& OutDependentObjects) override {
  }
  virtual bool NeedTransform_Implementation() override {
    return false;
  }
  virtual bool ShouldSave_Implementation() const override {
    return true;
  }

  void SeedMissingFromCatalog();
  void ForceReseedNeutralMatte();
  void ReseedAllFromCatalog();

  bool TryGet(FName Key, FPCSwatchEntry& Out) const;
  bool TryGetBySwatch(TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Swatch,
                      FPCSwatchEntry& Out) const;
  void Set(FName Key, const FPCSwatchEntry& Entry);
  void SetFromSlot(FName Key, const FFactoryCustomizationColorSlot& Slot);

  FName KeyFromSwatch(TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Swatch) const;
  static FName KeyFromSwatchStatic(TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Swatch);
  static bool IsPCCustomization(TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Swatch);

  void RebuildMaps();

  FPCSwatchEntryChanged& OnEntryChanged() {
    return EntryChanged;
  }

  UPROPERTY(SaveGame, ReplicatedUsing = OnRep_Entries)
  TArray<FPCSwatchEntry> Entries;

 protected:
  UFUNCTION()
  void OnRep_Entries();

 private:
  int32 FindIndex(FName Key) const;

  TMap<FName, int32> KeyToIndex;

  FPCSwatchEntryChanged EntryChanged;
};
