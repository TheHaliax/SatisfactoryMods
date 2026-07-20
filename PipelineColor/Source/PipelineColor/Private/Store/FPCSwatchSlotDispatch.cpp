// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Store/FPCSwatchSlotDispatch.h"

#include "Equipment/FGBuildGun.h"
#include "Equipment/FGBuildGunPaint.h"
#include "FGBlueprintFunctionLibrary.h"
#include "FGCustomizationRecipe.h"
#include "FGFactoryColoringTypes.h"
#include "FGGameState.h"
#include "FGPlayerController.h"
#include "Network/UPCChatRCO.h"
#include "Patching/NativeHookManager.h"
#include "PipelineColorLog.h"
#include "Store/APCSwatchStoreSubsystem.h"

namespace {
bool GHooksRegistered = false;

TMap<TWeakObjectPtr<AFGPlayerController>, TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch>>
    GActiveByPc;

void PruneDeadActiveEntries() {
  for (auto It = GActiveByPc.CreateIterator(); It; ++It) {
    if (!It.Key().IsValid()) {
      It.RemoveCurrent();
    }
  }
}

APCSwatchStoreSubsystem* StoreFor(UObject* WorldContext) {
  UWorld* World = IsValid(WorldContext) ? WorldContext->GetWorld() : nullptr;
  return APCSwatchStoreSubsystem::Find(World);
}

UPCChatRCO* LocalRco(UObject* WorldContext) {
  AFGPlayerController* PC = FPCSwatchSlotDispatch::ResolvePlayerController(WorldContext);
  if (!IsValid(PC) || !PC->IsLocalController()) {
    return nullptr;
  }
  return PC->GetRemoteCallObjectOfClass<UPCChatRCO>();
}

void SyncActivePcToServer(UObject* WorldContext,
                          TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Swatch) {
  UWorld* World = IsValid(WorldContext) ? WorldContext->GetWorld() : nullptr;
  if (!IsValid(World) || World->GetNetMode() != NM_Client) {
    return;
  }

  if (UPCChatRCO* Rco = LocalRco(WorldContext)) {
    Rco->Server_SetActivePcSwatch(Swatch);
  }
}

void SubmitPcColors(UObject* WorldContext,
                    TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Swatch,
                    const FFactoryCustomizationColorSlot& ColorData) {
  if (!APCSwatchStoreSubsystem::IsPCCustomization(Swatch)) {
    return;
  }

  UWorld* World = IsValid(WorldContext) ? WorldContext->GetWorld() : nullptr;
  if (!IsValid(World)) {
    return;
  }

  if (UPCChatRCO* Rco = LocalRco(WorldContext)) {
    Rco->Server_SetPcSwatchColors(Swatch, ColorData);
    return;
  }

  if (World->GetNetMode() != NM_Client) {
    APCSwatchStoreSubsystem* Store = APCSwatchStoreSubsystem::GetOrCreate(World);
    if (Store && Store->HasAuthority()) {
      const FName Key = APCSwatchStoreSubsystem::KeyFromSwatchStatic(Swatch);
      if (!Key.IsNone()) {
        Store->SetFromSlot(Key, ColorData);
      }
    }
  }
}

void RememberIfPc(TSubclassOf<UFGCustomizationRecipe> Recipe, UObject* WorldContext) {
  AFGPlayerController* PC = FPCSwatchSlotDispatch::ResolvePlayerController(WorldContext);
  if (!Recipe) {
    FPCSwatchSlotDispatch::SetActivePcDesc(PC, nullptr);
    SyncActivePcToServer(WorldContext, nullptr);
    return;
  }
  const TSubclassOf<UFGFactoryCustomizationDescriptor> Desc =
      UFGCustomizationRecipe::GetCustomizationDescriptor(Recipe);
  TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Swatch(Desc.Get());
  if (APCSwatchStoreSubsystem::IsPCCustomization(Swatch)) {
    FPCSwatchSlotDispatch::SetActivePcDesc(PC, Swatch);
    SyncActivePcToServer(WorldContext, Swatch);
  } else {
    FPCSwatchSlotDispatch::SetActivePcDesc(PC, nullptr);
    SyncActivePcToServer(WorldContext, nullptr);
  }
}

bool FillFromStore(TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Swatch,
                   UObject* WorldContext, FFactoryCustomizationColorSlot& Out) {
  APCSwatchStoreSubsystem* Store = StoreFor(WorldContext);
  if (!Store || !APCSwatchStoreSubsystem::IsPCCustomization(Swatch)) {
    return false;
  }

  FPCSwatchEntry Entry;
  if (!Store->TryGetBySwatch(Swatch, Entry)) {
    return false;
  }

  Out = Entry.ToSlot();
  if (!Out.PaintFinish) {
    const FSoftClassPath DefaultPath(
        TEXT("/Game/FactoryGame/Buildable/-Shared/Customization/PaintFinishes/"
             "PaintFinishDesc_Default.PaintFinishDesc_Default_C"));
    Out.PaintFinish = DefaultPath.TryLoadClass<UFGFactoryCustomizationDescriptor_PaintFinish>();
  }
  return true;
}

bool HandleCustomSlotWrite(UObject* WorldContext, uint8 SlotIdx,
                           const FFactoryCustomizationColorSlot& ColorData) {
  if (SlotIdx != INDEX_CUSTOM_COLOR_SLOT) {
    return false;
  }

  AFGPlayerController* PC = FPCSwatchSlotDispatch::ResolvePlayerController(WorldContext);
  const TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Active =
      FPCSwatchSlotDispatch::GetActivePcDesc(PC);
  if (!APCSwatchStoreSubsystem::IsPCCustomization(Active)) {
    return false;
  }

  SubmitPcColors(WorldContext, Active, ColorData);
  UE_LOG(LogPipelineColor, Log, TEXT("%s custom slot → RCO/store"), PIPELINECOLOR_LOG_PREFIX);
  return true;
}
} // namespace

AFGPlayerController* FPCSwatchSlotDispatch::ResolvePlayerController(UObject* WorldContext) {
  if (!IsValid(WorldContext)) {
    return nullptr;
  }

  if (AFGPlayerController* PC = Cast<AFGPlayerController>(WorldContext)) {
    return PC;
  }

  if (const AActor* Actor = Cast<AActor>(WorldContext)) {
    if (AFGPlayerController* PC = Cast<AFGPlayerController>(Actor->GetInstigatorController())) {
      return PC;
    }
    if (AFGPlayerController* PC = Cast<AFGPlayerController>(Actor->GetOwner())) {
      return PC;
    }
    if (const APawn* Pawn = Cast<APawn>(Actor->GetOwner())) {
      if (AFGPlayerController* PC = Cast<AFGPlayerController>(Pawn->GetController())) {
        return PC;
      }
    }
  }

  UWorld* World = WorldContext->GetWorld();
  if (!IsValid(World)) {
    return nullptr;
  }

  for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It) {
    if (AFGPlayerController* PC = Cast<AFGPlayerController>(It->Get())) {
      if (PC->IsLocalController()) {
        return PC;
      }
    }
  }
  return Cast<AFGPlayerController>(World->GetFirstPlayerController());
}

void FPCSwatchSlotDispatch::SetActivePcDesc(
    AFGPlayerController* PlayerController,
    TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Swatch) {
  PruneDeadActiveEntries();
  if (!IsValid(PlayerController)) {
    return;
  }

  if (APCSwatchStoreSubsystem::IsPCCustomization(Swatch)) {
    GActiveByPc.FindOrAdd(PlayerController) = Swatch;
  } else {
    GActiveByPc.Remove(PlayerController);
  }
}

TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch>
FPCSwatchSlotDispatch::GetActivePcDesc(AFGPlayerController* PlayerController) {
  PruneDeadActiveEntries();
  if (!IsValid(PlayerController)) {
    return nullptr;
  }
  if (const TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch>* Found =
          GActiveByPc.Find(PlayerController)) {
    return *Found;
  }
  return nullptr;
}

void FPCSwatchSlotDispatch::RegisterHooks() {
  if (GHooksRegistered) {
    return;
  }
  GHooksRegistered = true;

#if WITH_EDITOR
  return;
#else
  SUBSCRIBE_METHOD(UFGBlueprintFunctionLibrary::GetSlotDataForSwatchDesc,
                   [](auto& Scope, TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> SwatchDesc,
                      AActor* WorldContext, FFactoryCustomizationColorSlot& Out_SlotData) {
                     if (FillFromStore(SwatchDesc, WorldContext, Out_SlotData)) {
                       if (APCSwatchStoreSubsystem::IsPCCustomization(SwatchDesc)) {
                         AFGPlayerController* PC = ResolvePlayerController(WorldContext);
                         SetActivePcDesc(PC, SwatchDesc);
                         SyncActivePcToServer(WorldContext, SwatchDesc);
                       }
                       Scope.Cancel();
                     }
                   });

  SUBSCRIBE_METHOD_AFTER(
      UFGBuildGunStatePaint::SetActiveRecipe,
      [](UFGBuildGunStatePaint* Self, TSubclassOf<UFGCustomizationRecipe> Recipe) {
        RememberIfPc(Recipe, Self);
      });

  SUBSCRIBE_METHOD_AFTER(UFGBuildGunStatePaint::SetActiveSwatchDesc,
                         [](UFGBuildGunStatePaint* Self,
                            TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> SwatchDesc) {
                           AFGPlayerController* PC = ResolvePlayerController(Self);
                           if (APCSwatchStoreSubsystem::IsPCCustomization(SwatchDesc)) {
                             SetActivePcDesc(PC, SwatchDesc);
                             SyncActivePcToServer(Self, SwatchDesc);
                           } else {
                             SetActivePcDesc(PC, nullptr);
                             SyncActivePcToServer(Self, nullptr);
                           }
                         });

  SUBSCRIBE_METHOD(
      AFGBuildGun::SetCustomizationDataForSlot,
      [](auto& Scope, AFGBuildGun* Self, uint8 SlotIdx, FFactoryCustomizationColorSlot ColorData) {
        if (HandleCustomSlotWrite(Self, SlotIdx, ColorData)) {
          Scope.Cancel();
        }
      });

  SUBSCRIBE_METHOD_VIRTUAL(
      AFGBuildGun::Server_SetCustomizationDataForSlot_Implementation,
      GetMutableDefault<AFGBuildGun>(),
      [](auto& Scope, AFGBuildGun* Self, uint8 SlotIdx, FFactoryCustomizationColorSlot ColorData) {
        if (HandleCustomSlotWrite(Self, SlotIdx, ColorData)) {
          Scope.Cancel();
        }
      });

  SUBSCRIBE_METHOD_VIRTUAL(AFGGameState::Server_SetBuildingColorDataForSlot_Implementation,
                           GetMutableDefault<AFGGameState>(),
                           [](auto& Scope, AFGGameState* GameState, uint8 SlotIdx,
                              FFactoryCustomizationColorSlot ColorData) {
                             if (HandleCustomSlotWrite(GameState, SlotIdx, ColorData)) {
                               Scope.Cancel();
                             }
                           });

  UE_LOG(LogPipelineColor, Log, TEXT("%s swatch dispatch hooks registered"),
         PIPELINECOLOR_LOG_PREFIX);
#endif
}
