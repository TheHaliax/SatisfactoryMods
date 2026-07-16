// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "StructuralPowerRootInstanceModule.h"

#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableCircuitBridge.h"
#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Buildables/FGBuildableControlPanelHost.h"
#include "Buildables/FGBuildableFactory.h"
#include "Buildables/FGBuildableLightSource.h"
#include "Buildables/FGBuildableLightsControlPanel.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "Command/StructuralPowerSmlChatCommands.h"
#include "Config/FStructuralGroupToggleRegistry.h"
#include "Config/FStructuralPowerModConfig.h"
#include "Core/FStructuralPowerWorldGate.h"
#include "Diagnostics/FStructuralPowerTrace.h"
#include "Diagnostics/FStructuralVanillaPowerTrace.h"
#include "Equipment/FStructuralEquipmentBridgeRegistry.h"
#include "FGBuildableSubsystem.h"
#include "FGHUD.h"
#include "FGLightweightBuildableSubsystem.h"
#include "Graph/FStructuralEndpointDescriptor.h"
#include "Hologram/FGBlueprintHologram.h"
#include "Input/FStructuralPowerIdInput.h"
#include "Lightweight/FStructuralLightweightTypes.h"
#include "Network/UStructuralPowerRCO.h"
#include "Panel/FStructuralPanelControlledSync.h"
#include "Patching/NativeHookManager.h"
#include "Processors/FStructuralEndpointCatalog.h"
#include "Processors/FStructuralEndpointProcessors.h"
#include "Processors/IStructuralEndpointProcessor.h"
#include "Routing/EStructuralChannel.h"
#include "Rules/FStructuralEligibilityRules.h"
#include "Save/AStructuralPowerGraphSubsystem.h"
#include "StructuralPowerConstants.h"
#include "StructuralPowerLog.h"
#include "Subsystems/UStructuralPowerFactoryTickHandler.h"
#include "TimerManager.h"
#include "UI/FGGameUI.h"
#include "UI/FGInteractWidget.h"
#include "UI/FStructuralPowerIdPresenterFactory.h"

FDelegateHandle UStructuralPowerRootInstanceModule::PostLoadMapHandle;

UStructuralPowerRootInstanceModule::UStructuralPowerRootInstanceModule() {
  bRootModule = true;
  RemoteCallObjects.Add(UStructuralPowerRCO::StaticClass());
}

void UStructuralPowerRootInstanceModule::UnregisterGlobalDelegates() {
  if (PostLoadMapHandle.IsValid()) {
    FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
    PostLoadMapHandle.Reset();
  }
}

bool UStructuralPowerRootInstanceModule::TryEnqueueBuildable(AFGBuildable* Buildable,
                                                             const TCHAR* HookName,
                                                             const TCHAR* SourceTag) {
  if (!IsValid(Buildable)) {
    return false;
  }

  if (!Buildable->HasAuthority()) {
    FStructuralPowerTrace::LogPlacementSkip(Buildable, TEXT("no_authority"));
    return false;
  }

  UWorld* World = Buildable->GetWorld();
  if (!IsValid(World)) {
    FStructuralPowerTrace::LogPlacementSkip(Buildable, TEXT("no_world"));
    return false;
  }

  AStructuralPowerGraphSubsystem* Graph = AStructuralPowerGraphSubsystem::GetOrCreate(World);
  if (!Graph) {
    return false;
  }

  if (const IStructuralEndpointProcessor* Processor =
          FStructuralEndpointCatalog::Get().Classify(Buildable)) {
    const FStructuralEndpointDescriptor& Descriptor = Processor->GetDescriptor();
    FStructuralPowerTrace::LogHook(Buildable, HookName,
                                   StructuralEndpointKindToString(Descriptor.Kind), SourceTag);
    Graph->EnqueuePlacement(Buildable, Descriptor.PlacementJob, /*bDefer=*/true);
    return true;
  }

  if (FStructuralEligibilityRules::IsBusMember(Buildable)) {
    FStructuralPowerTrace::LogHook(Buildable, HookName, TEXT("enqueue_structure"), SourceTag);
    Graph->EnqueuePlacement(Buildable, EStructuralPlacementJobType::Structure, /*bDefer=*/true);
    return true;
  }

  if (FStructuralEligibilityRules::IsFluidPipeSupport(Buildable) ||
      FStructuralEligibilityRules::IsFluidPipeConductor(Buildable)) {
    FStructuralPowerTrace::LogHook(Buildable, HookName, TEXT("enqueue_pipe"), SourceTag);
    Graph->EnqueuePlacement(Buildable, EStructuralPlacementJobType::Pipe, /*bDefer=*/true);
    return true;
  }

  FStructuralPowerTrace::LogHook(Buildable, HookName, TEXT("ignored"), TEXT("not_bus_or_outlet"));
  return false;
}

static bool ShouldUseBulkBeginPlayLog(UWorld* World) {
  // BeginPlay fires before PostLoad rebuild / bulk drain flag — Log spam = hitch.
  if (!IsValid(World)) {
    return true;
  }

  const AStructuralPowerGraphSubsystem* Graph = AStructuralPowerGraphSubsystem::Find(World);
  if (!Graph) {
    return true;
  }

  return !Graph->IsPostLoadRebuilt() || Graph->IsBulkLoadDrainActive();
}

static void HandlePoleBeginPlay(AFGBuildablePowerPole* Pole) {
  if (!IsValid(Pole) || !Pole->HasAuthority()) {
    return;
  }

  UWorld* World = Pole->GetWorld();
  if (!IsValid(World)) {
    return;
  }

  if (ShouldUseBulkBeginPlayLog(World)) {
    UE_LOG(LogStructuralPower, Verbose, TEXT("[HALSP] pole BeginPlay %s — enqueue outlet"),
           *Pole->GetName());
  } else {
    UE_LOG(LogStructuralPower, Log, TEXT("[HALSP] pole BeginPlay %s — enqueue outlet"),
           *Pole->GetName());
  }

  World->GetTimerManager().SetTimerForNextTick(
      FTimerDelegate::CreateLambda([WorldWeak = TWeakObjectPtr<UWorld>(World),
                                    PoleWeak = TWeakObjectPtr<AFGBuildablePowerPole>(Pole)]() {
        if (AFGBuildablePowerPole* PolePtr = PoleWeak.Get()) {
          if (UWorld* WorldPtr = WorldWeak.Get()) {
            if (AStructuralPowerGraphSubsystem* Graph =
                    AStructuralPowerGraphSubsystem::GetOrCreate(WorldPtr)) {
              Graph->EnqueuePlacement(PolePtr, EStructuralPlacementJobType::Outlet,
                                      /*bDefer=*/true);
            }
          }
        }
      }));
}

static void HandleSwitchBeginPlay(AFGBuildableCircuitSwitch* Switch) {
  if (!IsValid(Switch) || !Switch->HasAuthority()) {
    return;
  }

  UWorld* World = Switch->GetWorld();
  if (!IsValid(World)) {
    return;
  }

  if (ShouldUseBulkBeginPlayLog(World)) {
    UE_LOG(LogStructuralPower, Verbose, TEXT("[HALSP] switch BeginPlay %s — enqueue outlet"),
           *Switch->GetName());
  } else {
    UE_LOG(LogStructuralPower, Log, TEXT("[HALSP] switch BeginPlay %s — enqueue outlet"),
           *Switch->GetName());
  }

  World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda(
      [WorldWeak = TWeakObjectPtr<UWorld>(World),
       SwitchWeak = TWeakObjectPtr<AFGBuildableCircuitSwitch>(Switch)]() {
        if (AFGBuildableCircuitSwitch* SwitchPtr = SwitchWeak.Get()) {
          if (UWorld* WorldPtr = WorldWeak.Get()) {
            if (AStructuralPowerGraphSubsystem* Graph =
                    AStructuralPowerGraphSubsystem::GetOrCreate(WorldPtr)) {
              Graph->EnqueuePlacement(SwitchPtr, EStructuralPlacementJobType::Outlet,
                                      /*bDefer=*/true);
            }
          }
        }
      }));
}

static void EnqueuePanelOutletWhenReady(TWeakObjectPtr<UWorld> WorldWeak,
                                        TWeakObjectPtr<AFGBuildableLightsControlPanel> PanelWeak) {
  AFGBuildableLightsControlPanel* PanelPtr = PanelWeak.Get();
  UWorld* WorldPtr = WorldWeak.Get();
  if (!IsValid(PanelPtr) || !IsValid(WorldPtr)) {
    return;
  }

  AStructuralPowerGraphSubsystem* Graph = AStructuralPowerGraphSubsystem::GetOrCreate(WorldPtr);
  if (!Graph) {
    return;
  }

  if (Graph->ShouldDeferCircuitDrivenRefresh()) {
    WorldPtr->GetTimerManager().SetTimerForNextTick(
        FTimerDelegate::CreateStatic(&EnqueuePanelOutletWhenReady, WorldWeak, PanelWeak));
    return;
  }

  Graph->EnqueuePlacement(PanelPtr, EStructuralPlacementJobType::Outlet,
                          /*bDefer=*/true);
}

static void HandlePanelBeginPlay(AFGBuildableLightsControlPanel* Panel) {
  if (!IsValid(Panel) || !Panel->HasAuthority()) {
    return;
  }

  UWorld* World = Panel->GetWorld();
  if (!IsValid(World)) {
    return;
  }

  if (ShouldUseBulkBeginPlayLog(World)) {
    UE_LOG(LogStructuralPower, Verbose, TEXT("[HALSP] panel BeginPlay %s — enqueue outlet"),
           *Panel->GetName());
  } else {
    UE_LOG(LogStructuralPower, Log, TEXT("[HALSP] panel BeginPlay %s — enqueue outlet"),
           *Panel->GetName());
  }

  World->GetTimerManager().SetTimerForNextTick(
      FTimerDelegate::CreateStatic(&EnqueuePanelOutletWhenReady, TWeakObjectPtr<UWorld>(World),
                                   TWeakObjectPtr<AFGBuildableLightsControlPanel>(Panel)));
}

static void HandleLightBeginPlay(AFGBuildableLightSource* Light) {
  if (!IsValid(Light) || !Light->HasAuthority()) {
    return;
  }

  UWorld* World = Light->GetWorld();
  if (!IsValid(World)) {
    return;
  }

  if (ShouldUseBulkBeginPlayLog(World)) {
    UE_LOG(LogStructuralPower, Verbose, TEXT("[HALSP] light BeginPlay %s — enqueue outlet"),
           *Light->GetName());
  } else {
    UE_LOG(LogStructuralPower, Log, TEXT("[HALSP] light BeginPlay %s — enqueue outlet"),
           *Light->GetName());
  }

  World->GetTimerManager().SetTimerForNextTick(
      FTimerDelegate::CreateLambda([WorldWeak = TWeakObjectPtr<UWorld>(World),
                                    LightWeak = TWeakObjectPtr<AFGBuildableLightSource>(Light)]() {
        if (AFGBuildableLightSource* LightPtr = LightWeak.Get()) {
          if (UWorld* WorldPtr = WorldWeak.Get()) {
            if (AStructuralPowerGraphSubsystem* Graph =
                    AStructuralPowerGraphSubsystem::GetOrCreate(WorldPtr)) {
              Graph->EnqueuePlacement(LightPtr, EStructuralPlacementJobType::Outlet,
                                      /*bDefer=*/true);
            }
          }
        }
      }));
}

static void HandleLightBuildEffectFinished(AFGBuildableLightSource* Light) {
  if (!IsValid(Light) || !Light->HasAuthority()) {
    return;
  }

  UWorld* World = Light->GetWorld();
  if (!IsValid(World)) {
    return;
  }

  if (AStructuralPowerGraphSubsystem* Graph = AStructuralPowerGraphSubsystem::GetOrCreate(World)) {
    FStructuralPowerTrace::LogHook(Light, TEXT("OnBuildEffectFinished"), TEXT("enqueue_light"),
                                   TEXT("defer"));
    Graph->EnqueuePlacement(Light, EStructuralPlacementJobType::Outlet, /*bDefer=*/true);
  }
}

void UStructuralPowerRootInstanceModule::HandleBuildableBuilt(AFGBuildable* Buildable) {
  TryEnqueueBuildable(Buildable, TEXT("OnBuildEffectFinished"), TEXT("defer"));
}

static void HandleFactoryMachineBeginPlay(AFGBuildableFactory* Factory) {
  if (!IsValid(Factory) || !Factory->HasAuthority()) {
    return;
  }

  // OnBuildEffectFinished sometimes skips factories (miners). Next-tick enqueue
  // mirrors poles — never sync Process in BeginPlay (GetOrCreate AV risk).
  if (!FStructuralEligibilityRules::IsStructuralGenerator(Factory) &&
      !FStructuralEligibilityRules::IsStructuralExtractor(Factory) &&
      !FStructuralEligibilityRules::IsStructuralManufacturer(Factory) &&
      !FStructuralEligibilityRules::IsStructuralTransport(Factory) &&
      !FStructuralEligibilityRules::IsStructuralPipelinePump(Factory) &&
      !FStructuralEligibilityRules::IsPowerStorage(Factory)) {
    return;
  }

  UWorld* World = Factory->GetWorld();
  if (!IsValid(World)) {
    return;
  }

  World->GetTimerManager().SetTimerForNextTick(
      FTimerDelegate::CreateLambda([WorldWeak = TWeakObjectPtr<UWorld>(World),
                                    FactoryWeak = TWeakObjectPtr<AFGBuildableFactory>(Factory)]() {
        AFGBuildableFactory* FactoryPtr = FactoryWeak.Get();
        UWorld* WorldPtr = WorldWeak.Get();
        if (!IsValid(FactoryPtr) || !IsValid(WorldPtr)) {
          return;
        }

        if (AStructuralPowerGraphSubsystem* Graph =
                AStructuralPowerGraphSubsystem::GetOrCreate(WorldPtr)) {
          UE_LOG(LogStructuralPower, Log, TEXT("[HALSP] FactoryBeginPlay enqueue %s"),
                 *FactoryPtr->GetName());
          Graph->EnqueuePlacement(FactoryPtr, EStructuralPlacementJobType::Outlet,
                                  /*bDefer=*/true);
        }
      }));
}

void UStructuralPowerRootInstanceModule::HandleBuildablesConstructed(
    const TArray<AActor*>& Children) {
  int32 Enqueued = 0;
  for (AActor* Child : Children) {
    if (AFGBuildable* Buildable = Cast<AFGBuildable>(Child)) {
      if (TryEnqueueBuildable(Buildable, TEXT("BlueprintConstruct"), TEXT("defer"))) {
        ++Enqueued;
      }
    }
  }

  if (Enqueued > 0) {
    UE_LOG(LogStructuralPower, Log, TEXT("[HALSP] BlueprintConstruct deferred %d buildable(s)"),
           Enqueued);
  }
}

static void HandleSwitchCircuitsRebuilt(AFGBuildableCircuitBridge* Bridge) {
  AFGBuildableCircuitSwitch* Switch = Cast<AFGBuildableCircuitSwitch>(Bridge);
  if (!IsValid(Switch) || !Switch->HasAuthority()) {
    return;
  }

  UWorld* World = Switch->GetWorld();
  if (!IsValid(World)) {
    return;
  }

  World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda(
      [WorldWeak = TWeakObjectPtr<UWorld>(World),
       SwitchWeak = TWeakObjectPtr<AFGBuildableCircuitSwitch>(Switch)]() {
        if (AFGBuildableCircuitSwitch* SwitchPtr = SwitchWeak.Get()) {
          if (UWorld* WorldPtr = WorldWeak.Get()) {
            if (AStructuralPowerGraphSubsystem* Graph =
                    AStructuralPowerGraphSubsystem::GetOrCreate(WorldPtr)) {
              if (Graph->ShouldDeferCircuitDrivenRefresh()) {
                WorldPtr->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateStatic(
                    &HandleSwitchCircuitsRebuilt,
                    static_cast<AFGBuildableCircuitBridge*>(SwitchPtr)));
                return;
              }

              Graph->ProcessSwitchCircuitsRebuilt(SwitchPtr);
            }
          }
        }
      }));
}

void UStructuralPowerRootInstanceModule::HandlePowerConnectionChanged(
    AFGBuildablePowerPole* Pole, UFGCircuitConnectionComponent* /*Connection*/) {
  if (!IsValid(Pole) || !Pole->HasAuthority()) {
    return;
  }

  if (!FStructuralEligibilityRules::IsPowerBridgePole(Pole)) {
    return;
  }

  UWorld* World = Pole->GetWorld();
  if (!IsValid(World)) {
    return;
  }

  World->GetTimerManager().SetTimerForNextTick(
      FTimerDelegate::CreateLambda([WorldWeak = TWeakObjectPtr<UWorld>(World),
                                    PoleWeak = TWeakObjectPtr<AFGBuildablePowerPole>(Pole)]() {
        if (AFGBuildablePowerPole* PolePtr = PoleWeak.Get()) {
          if (UWorld* WorldPtr = WorldWeak.Get()) {
            if (AStructuralPowerGraphSubsystem* Graph =
                    AStructuralPowerGraphSubsystem::GetOrCreate(WorldPtr)) {
              Graph->ProcessWallOutletAfterWire(PolePtr);
            }
          }
        }
      }));
}

void UStructuralPowerRootInstanceModule::HandleBuildableRemoved(AFGBuildable* Buildable) {
  if (!IsValid(Buildable)) {
    return;
  }

  if (UWorld* World = Buildable->GetWorld()) {
    if (AStructuralPowerGraphSubsystem* Graph = AStructuralPowerGraphSubsystem::Find(World)) {
      Graph->OnBuildableRemoved(Buildable);
    }
  }
}

void UStructuralPowerRootInstanceModule::HandleLightweightMemberAdded(
    AFGLightweightBuildableSubsystem* Subsystem, TSubclassOf<AFGBuildable> BuildableClass,
    int32 InstanceIndex) {
  if (!IsValid(Subsystem) || InstanceIndex == INDEX_NONE || !BuildableClass) {
    return;
  }

  const AFGBuildable* CDO = BuildableClass->GetDefaultObject<AFGBuildable>();
  if (!FStructuralEligibilityRules::IsBusMember(CDO)) {
    return;
  }

  UWorld* World = Subsystem->GetWorld();
  if (!IsValid(World) || World->GetNetMode() == NM_Client) {
    return;
  }

  const FStructuralLightweightKey Key{BuildableClass, InstanceIndex};
  UE_LOG(LogStructuralPower, Log, TEXT("[HALSP] LW hook AddFromBuildableInstanceData %s[%d]"),
         *BuildableClass->GetName(), InstanceIndex);

  World->GetTimerManager().SetTimerForNextTick(
      FTimerDelegate::CreateLambda([WorldWeak = TWeakObjectPtr<UWorld>(World), Key]() {
        if (UWorld* WorldPtr = WorldWeak.Get()) {
          if (AStructuralPowerGraphSubsystem* Graph =
                  AStructuralPowerGraphSubsystem::GetOrCreate(WorldPtr)) {
            Graph->EnqueueLightweightPlacement(Key, /*bDefer=*/true);
          }
        }
      }));
}

void UStructuralPowerRootInstanceModule::HandleLightweightMemberRemoved(
    AFGLightweightBuildableSubsystem* Subsystem, TSubclassOf<AFGBuildable> BuildableClass,
    int32 InstanceIndex) {
  if (!IsValid(Subsystem) || InstanceIndex == INDEX_NONE || !BuildableClass) {
    return;
  }

  UE_LOG(LogStructuralPower, Verbose, TEXT("[HALSP] LW hook RemoveByInstanceIndex %s[%d]"),
         *BuildableClass->GetName(), InstanceIndex);

  if (UWorld* World = Subsystem->GetWorld()) {
    if (AStructuralPowerGraphSubsystem* Graph = AStructuralPowerGraphSubsystem::Find(World)) {
      Graph->OnLightweightRemoved(FStructuralLightweightKey{BuildableClass, InstanceIndex});
    }
  }
}

void UStructuralPowerRootInstanceModule::HandlePostLoadMap(UWorld* World) {
  if (!FStructuralPowerWorldGate::IsGameplayWorld(World)) {
    return;
  }

  FStructuralPowerIdPresenterFactory::ResetForMapTravel();

  World->GetTimerManager().SetTimerForNextTick(
      FTimerDelegate::CreateLambda([WorldWeak = TWeakObjectPtr<UWorld>(World)]() {
        if (UWorld* WorldPtr = WorldWeak.Get()) {
          if (!FStructuralPowerWorldGate::IsGameplayWorld(WorldPtr)) {
            return;
          }

          if (AStructuralPowerGraphSubsystem* Graph =
                  AStructuralPowerGraphSubsystem::GetOrCreate(WorldPtr)) {
            Graph->OnWorldReady(WorldPtr);
          }

          FStructuralPowerSmlChatCommands::RegisterWithWorld(WorldPtr);
        }
      }));
}

void UStructuralPowerRootInstanceModule::DispatchLifecycleEvent(ELifecyclePhase Phase) {
  if (Phase == ELifecyclePhase::POST_INITIALIZATION) {
    FStructuralPowerModConfig::LoadRuntimeConfig();
#if !WITH_EDITOR
    UE_LOG(LogStructuralPower, Log,
           TEXT("StructuralPower v3.1.0 — machines + vanilla-first reconcile (I-key Id panel)"
                " (groupLighting=%s trace=%s extendedDebug=%s)"),
           FStructuralPowerModConfig::IsGroupLightingEnabled() ? TEXT("on") : TEXT("off"),
           FStructuralPowerModConfig::IsTraceEnabled() ? TEXT("on") : TEXT("off"),
           FStructuralPowerModConfig::IsExtendedDebugEnabled() ? TEXT("on") : TEXT("off"));
#endif
  }

  if (Phase != ELifecyclePhase::INITIALIZATION) {
    Super::DispatchLifecycleEvent(Phase);
    return;
  }

#if WITH_EDITOR
  UE_LOG(LogStructuralPower, Log, TEXT("StructuralPower: skipping hooks in editor"));
#else
  FStructuralEquipmentBridgeRegistry::Get().Initialize();
  FStructuralEndpointProcessors::InitializeRegistries();
  FStructuralGroupToggleRegistry::Get().Initialize();
  FStructuralPowerIdInput::Register();
  FStructuralVanillaPowerTrace::RegisterHooks();

  SUBSCRIBE_METHOD_AFTER(
      UFGGameUI::RemoveInteractWidget, [](UFGGameUI* GameUI, UFGInteractWidget* /*Widget*/) {
        if (!IsValid(GameUI)) {
          return;
        }

        if (AFGPlayerController* PC = Cast<AFGPlayerController>(GameUI->GetOwningPlayer())) {
          FStructuralPowerIdInput::RecoverAfterVanillaUiClosed(PC);
        }
      });

  SUBSCRIBE_METHOD_AFTER(AFGHUD::OpenInteractUI, [](AFGHUD* Hud,
                                                    TSubclassOf<UFGInteractWidget> /*WidgetClass*/,
                                                    UObject* /*InteractObject*/) {
    FStructuralPowerIdPresenterFactory::CloseActive();
    if (IsValid(Hud)) {
      if (AFGPlayerController* PC = Cast<AFGPlayerController>(Hud->GetOwningPlayerController())) {
        FStructuralPowerIdPresenterFactory::ReleaseForVanillaInteract(PC);
      }
    }
  });

  SUBSCRIBE_METHOD_VIRTUAL(
      AFGBuildableControlPanelHost::BeginPlay, GetMutableDefault<AFGBuildableControlPanelHost>(),
      [](auto& /*Scope*/, AFGBuildableControlPanelHost* Host) {
        if (!IsValid(Host) || !Host->HasAuthority()) {
          return;
        }

        if (Host->IsA<AFGBuildableLightsControlPanel>()) {
          AStructuralPowerGraphSubsystem::StripPersistedEndpointModComponents(Host);
        }
      });

  SUBSCRIBE_METHOD_VIRTUAL_AFTER(
      AFGBuildableControlPanelHost::BeginPlay, GetMutableDefault<AFGBuildableControlPanelHost>(),
      [](AFGBuildableControlPanelHost* Host) {
        if (AFGBuildableLightsControlPanel* Panel = Cast<AFGBuildableLightsControlPanel>(Host)) {
          HandlePanelBeginPlay(Panel);
        }
      });

  SUBSCRIBE_METHOD_VIRTUAL_AFTER(
      AFGBuildableLightSource::BeginPlay, GetMutableDefault<AFGBuildableLightSource>(),
      [](AFGBuildableLightSource* Light) { HandleLightBeginPlay(Light); });

  SUBSCRIBE_METHOD_VIRTUAL_AFTER(
      AFGBuildableLightSource::OnBuildEffectFinished, GetMutableDefault<AFGBuildableLightSource>(),
      [](AFGBuildableLightSource* Light) { HandleLightBuildEffectFinished(Light); });

  SUBSCRIBE_METHOD_VIRTUAL(
      AFGBuildableCircuitSwitch::BeginPlay, GetMutableDefault<AFGBuildableCircuitSwitch>(),
      [](auto& /*Scope*/, AFGBuildableCircuitSwitch* Switch) {
        if (!IsValid(Switch) || !Switch->HasAuthority()) {
          return;
        }

        AStructuralPowerGraphSubsystem::StripPersistedEndpointModComponents(Switch);
      });

  SUBSCRIBE_METHOD_VIRTUAL_AFTER(AFGBuildablePowerPole::BeginPlay,
                                 GetMutableDefault<AFGBuildablePowerPole>(),
                                 [](AFGBuildablePowerPole* Pole) { HandlePoleBeginPlay(Pole); });

  SUBSCRIBE_METHOD_VIRTUAL_AFTER(
      AFGBuildableCircuitSwitch::BeginPlay, GetMutableDefault<AFGBuildableCircuitSwitch>(),
      [](AFGBuildableCircuitSwitch* Switch) { HandleSwitchBeginPlay(Switch); });

  SUBSCRIBE_METHOD_VIRTUAL_AFTER(AFGBuildable::OnBuildEffectFinished,
                                 GetMutableDefault<AFGBuildable>(),
                                 [](AFGBuildable* Buildable) { HandleBuildableBuilt(Buildable); });

  SUBSCRIBE_METHOD_VIRTUAL_AFTER(
      AFGBuildableFactory::BeginPlay, GetMutableDefault<AFGBuildableFactory>(),
      [](AFGBuildableFactory* Factory) { HandleFactoryMachineBeginPlay(Factory); });

  SUBSCRIBE_METHOD_VIRTUAL_AFTER(
      AFGBlueprintHologram::Construct, GetMutableDefault<AFGBlueprintHologram>(),
      [](AActor* /*ReturnValue*/, AFGBlueprintHologram* /*Hologram*/, TArray<AActor*>& OutChildren,
         FNetConstructionID /*NetConstructionID*/) { HandleBuildablesConstructed(OutChildren); });

  SUBSCRIBE_METHOD_AFTER(AFGBuildableSubsystem::RemoveBuildable,
                         [](AFGBuildableSubsystem* /*Subsystem*/, AFGBuildable* Buildable) {
                           HandleBuildableRemoved(Buildable);
                         });

  SUBSCRIBE_METHOD_VIRTUAL_AFTER(
      AFGBuildableCircuitBridge::OnCircuitsRebuilt, GetMutableDefault<AFGBuildableCircuitBridge>(),
      [](AFGBuildableCircuitBridge* Bridge) { HandleSwitchCircuitsRebuilt(Bridge); });

  SUBSCRIBE_METHOD_VIRTUAL_AFTER(
      AFGBuildablePowerPole::OnPowerConnectionChanged, GetMutableDefault<AFGBuildablePowerPole>(),
      [](AFGBuildablePowerPole* Pole, UFGCircuitConnectionComponent* Connection) {
        HandlePowerConnectionChanged(Pole, Connection);
      });

  SUBSCRIBE_METHOD_AFTER(
      AFGLightweightBuildableSubsystem::AddFromBuildableInstanceData,
      [](int32 ReturnValue, AFGLightweightBuildableSubsystem* Subsystem,
         TSubclassOf<AFGBuildable> BuildableClass,
         FRuntimeBuildableInstanceData& /*BuildableInstanceData*/, bool FromSaveData,
         int32 /*SaveDataBuildableIndex*/, uint16 /*ConstructId*/,
         AActor* /*BuildEffectInstigator*/, int32 /*BlueprintBuildEffectIndex*/) {
        if (FromSaveData) {
          return;
        }

        HandleLightweightMemberAdded(Subsystem, BuildableClass, ReturnValue);
      });

  SUBSCRIBE_METHOD_AFTER(AFGLightweightBuildableSubsystem::RemoveByInstanceIndex,
                         [](AFGLightweightBuildableSubsystem* Subsystem,
                            TSubclassOf<AFGBuildable> BuildableClass, int32 InstanceIndex) {
                           HandleLightweightMemberRemoved(Subsystem, BuildableClass, InstanceIndex);
                         });

  if (!PostLoadMapHandle.IsValid()) {
    PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddStatic(&HandlePostLoadMap);
  }
#endif

  Super::DispatchLifecycleEvent(Phase);
}
