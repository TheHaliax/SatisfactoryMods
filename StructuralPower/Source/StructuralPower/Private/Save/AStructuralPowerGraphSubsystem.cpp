// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Save/AStructuralPowerGraphSubsystem.h"

#include "Attach/FStructuralDeviceAttach.h"
#include "Attach/FStructuralPanelAttach.h"
#include "Attach/FStructuralPowerTransferGate.h"
#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableCircuitBridge.h"
#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Buildables/FGBuildableGenerator.h"
#include "Buildables/FGBuildableLightSource.h"
#include "Buildables/FGBuildableLightsControlPanel.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "Buildables/FGBuildablePowerStorage.h"
#include "Circuit/FStructuralCircuitPromotionScope.h"
#include "Circuit/FStructuralCircuitPromotionUtil.h"
#include "Components/UFGStructuralPowerConnectionComponent.h"
#include "Config/FStructuralPowerModConfig.h"
#include "Core/FStructuralPowerWorldGate.h"
#include "Diagnostics/FStructuralPowerDiagnostics.h"
#include "Diagnostics/FStructuralPowerTrace.h"
#include "Engine/World.h"
#include "Equipment/FStructuralEquipmentBridgeRegistry.h"
#include "FGBuildableSubsystem.h"
#include "FGCircuitConnectionComponent.h"
#include "FGCircuitSubsystem.h"
#include "FGLightweightBuildableSubsystem.h"
#include "FGPowerConnectionComponent.h"
#include "Graph/FStructuralAttachmentResolver.h"
#include "Graph/FStructuralBusMemberSpatialIndex.h"
#include "Graph/FStructuralEndpointIndex.h"
#include "Graph/FStructuralOutletParentResolver.h"
#include "Graph/FStructuralPowerBuildableCasts.h"
#include "Graph/FStructuralSwitchParentResolver.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Lightweight/FStructuralLightweightIndex.h"
#include "Network/UStructuralPowerMachineWireListener.h"
#include "Network/UStructuralPowerPanelListener.h"
#include "Network/UStructuralPowerStorageListener.h"
#include "Network/UStructuralPowerSwitchListener.h"
#include "Panel/FStructuralPanelControlledSync.h"
#include "Panel/FStructuralPanelPortResolver.h"
#include "Processors/FStructuralEndpointCatalog.h"
#include "Processors/FStructuralEndpointDispatcher.h"
#include "Processors/FStructuralEndpointProcessors.h"
#include "Processors/FStructuralPowerGeneratorProcessor.h"
#include "Processors/FStructuralPowerLightProcessor.h"
#include "Processors/FStructuralPowerPanelProcessor.h"
#include "Routing/FStructuralMembershipRole.h"
#include "Routing/FStructuralPowerRouter.h"
#include "Rules/FStructuralEligibilityRules.h"
#include "StructuralPowerConstants.h"
#include "StructuralPowerLog.h"
#include "Subsystems/UStructuralPowerFactoryTickHandler.h"
#include "TimerManager.h"

AStructuralPowerGraphSubsystem::AStructuralPowerGraphSubsystem() {
  bReplicates = false;
  IdRegistry.Bind(ComponentDefaultIds, PlayerEndpointOverrides, NextStructureDefaultIdIndex);
}

FStructuralGraphSession& AStructuralPowerGraphSubsystem::GetGraphSession() {
  if (!GraphSession.IsValid()) {
    GraphSession = MakeUnique<FStructuralGraphSession>(*this);
  }
  if (!bOpsBoundToSession) {
    FStructuralGraphSession& Session = *GraphSession;
    ReconcileOps.Bind(&Session);
    RestitchOps.Bind(&Session);
    CircuitOps.Bind(&Session);
    BridgeRootIndex.Bind(&Session);
    BootstrapOps.Bind(&Session);
    StructureIngressOps.Bind(&Session);
    BulkDrainOps.Bind(&Session);
    CircuitEchoOps.Bind(&Session);
    RemovalOps.Bind(&Session);
    IdOps.Bind(&Session);
    bOpsBoundToSession = true;
  }
  return *GraphSession;
}

const FStructuralGraphSession& AStructuralPowerGraphSubsystem::GetGraphSession() const {
  return const_cast<AStructuralPowerGraphSubsystem*>(this)->GetGraphSession();
}

AStructuralPowerGraphSubsystem* AStructuralPowerGraphSubsystem::GetOrCreate(UWorld* World) {
  if (!FStructuralPowerWorldGate::IsGameplayWorld(World) || World->GetNetMode() == NM_Client) {
    return nullptr;
  }

  if (AStructuralPowerGraphSubsystem* Existing =
          Cast<AStructuralPowerGraphSubsystem>(UGameplayStatics::GetActorOfClass(
              World, AStructuralPowerGraphSubsystem::StaticClass()))) {
    return Existing;
  }

  FActorSpawnParameters SpawnParams;
  SpawnParams.Name = TEXT("StructuralPowerGraphSubsystem");
  SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

  return World->SpawnActor<AStructuralPowerGraphSubsystem>(
      AStructuralPowerGraphSubsystem::StaticClass(), FTransform::Identity, SpawnParams);
}

AStructuralPowerGraphSubsystem* AStructuralPowerGraphSubsystem::Find(UWorld* World) {
  if (!IsValid(World) || World->GetNetMode() == NM_Client) {
    return nullptr;
  }

  return Cast<AStructuralPowerGraphSubsystem>(
      UGameplayStatics::GetActorOfClass(World, AStructuralPowerGraphSubsystem::StaticClass()));
}

FStructuralNodeId AStructuralPowerGraphSubsystem::MakeNodeId(const AFGBuildable* Buildable) {
  FStructuralNodeId Id;
  if (!IsValid(Buildable)) {
    return Id;
  }

  Id.BuildableClass = FSoftClassPath(Buildable->GetClass());
  Id.ActorName = Buildable->GetFName();
  return Id;
}

UFGStructuralPowerConnectionComponent* AStructuralPowerGraphSubsystem::FindBusConnector(
    const AFGBuildable* Host) {
  if (!IsValid(Host)) {
    return nullptr;
  }

  TInlineComponentArray<UFGStructuralPowerConnectionComponent*> Connectors;
  const_cast<AFGBuildable*>(Host)->GetComponents(Connectors);
  for (UFGStructuralPowerConnectionComponent* Connector : Connectors) {
    if (IsValid(Connector) &&
        Connector->GetFName() == StructuralPowerConstants::OutletBusConnectorName) {
      return Connector;
    }
  }

  return nullptr;
}

UFGStructuralPowerConnectionComponent* AStructuralPowerGraphSubsystem::FindPanelControlBus(
    const AFGBuildable* Host) {
  if (!IsValid(Host)) {
    return nullptr;
  }

  TInlineComponentArray<UFGStructuralPowerConnectionComponent*> Connectors;
  const_cast<AFGBuildable*>(Host)->GetComponents(Connectors);
  for (UFGStructuralPowerConnectionComponent* Connector : Connectors) {
    if (IsValid(Connector) &&
        Connector->GetFName() == StructuralPowerConstants::PanelControlBusConnectorName) {
      return Connector;
    }
  }

  return nullptr;
}

UFGStructuralPowerConnectionComponent* AStructuralPowerGraphSubsystem::FindSwitchControlBus(
    const AFGBuildable* Host) {
  if (!IsValid(Host)) {
    return nullptr;
  }

  TInlineComponentArray<UFGStructuralPowerConnectionComponent*> Connectors;
  const_cast<AFGBuildable*>(Host)->GetComponents(Connectors);
  for (UFGStructuralPowerConnectionComponent* Connector : Connectors) {
    if (IsValid(Connector) &&
        Connector->GetFName() == StructuralPowerConstants::SwitchControlBusConnectorName) {
      return Connector;
    }
  }

  return nullptr;
}

void AStructuralPowerGraphSubsystem::StripPersistedEndpointModComponents(AFGBuildable* Host) {
  // Saved outlet/panel buses still reference torn mesh — strip before live restitch or load
  // asserts.
  if (!IsValid(Host) || !Host->HasAuthority()) {
    return;
  }

  TInlineComponentArray<UFGStructuralPowerConnectionComponent*> Buses;
  Host->GetComponents(Buses);
  for (UFGStructuralPowerConnectionComponent* Bus : Buses) {
    if (!IsValid(Bus)) {
      continue;
    }

    const FName BusName = Bus->GetFName();
    if (BusName != StructuralPowerConstants::OutletBusConnectorName &&
        BusName != StructuralPowerConstants::PanelControlBusConnectorName &&
        BusName != StructuralPowerConstants::SwitchControlBusConnectorName) {
      continue;
    }

    TArray<UFGCircuitConnectionComponent*> HiddenLinks;
    Bus->GetHiddenConnections(HiddenLinks);
    for (UFGCircuitConnectionComponent* Other : HiddenLinks) {
      if (IsValid(Other)) {
        Other->RemoveHiddenConnection(Bus);
      }
      Bus->RemoveHiddenConnection(Other);
    }
    Bus->ClearHiddenConnections();

    if (UWorld* World = Host->GetWorld()) {
      if (AFGCircuitSubsystem* CircuitSubsystem = AFGCircuitSubsystem::Get(World)) {
        CircuitSubsystem->RemoveComponent(Bus);
      }
    }

    Host->RemoveInstanceComponent(Bus);
    Bus->DestroyComponent();
  }

  if (Host->IsA<AFGBuildableCircuitSwitch>()) {
    TInlineComponentArray<UStructuralPowerSwitchListener*> Listeners;
    Host->GetComponents(Listeners);
    for (UStructuralPowerSwitchListener* Listener : Listeners) {
      if (!IsValid(Listener)) {
        continue;
      }

      Host->RemoveInstanceComponent(Listener);
      Listener->DestroyComponent();
    }
  }

  if (Host->IsA<AFGBuildableLightsControlPanel>()) {
    TInlineComponentArray<UStructuralPowerPanelListener*> Listeners;
    Host->GetComponents(Listeners);
    for (UStructuralPowerPanelListener* Listener : Listeners) {
      if (!IsValid(Listener)) {
        continue;
      }

      Host->RemoveInstanceComponent(Listener);
      Listener->DestroyComponent();
    }
  }

  if (Host->IsA<AFGBuildablePowerStorage>()) {
    TInlineComponentArray<UStructuralPowerStorageListener*> Listeners;
    Host->GetComponents(Listeners);
    for (UStructuralPowerStorageListener* Listener : Listeners) {
      if (!IsValid(Listener)) {
        continue;
      }

      Host->RemoveInstanceComponent(Listener);
      Listener->DestroyComponent();
    }
  }

  {
    TInlineComponentArray<UStructuralPowerMachineWireListener*> Listeners;
    Host->GetComponents(Listeners);
    for (UStructuralPowerMachineWireListener* Listener : Listeners) {
      if (!IsValid(Listener)) {
        continue;
      }

      Host->RemoveInstanceComponent(Listener);
      Listener->DestroyComponent();
    }
  }
}

UFGStructuralPowerConnectionComponent* AStructuralPowerGraphSubsystem::FindOutletBusConnector(
    const AFGBuildablePowerPole* Outlet) {
  return FindBusConnector(Outlet);
}

UFGStructuralPowerConnectionComponent* AStructuralPowerGraphSubsystem::GetOrCreateBusConnector(
    AFGBuildable* Host) {
  if (!IsValid(Host)) {
    return nullptr;
  }

  const bool bPole = Host->IsA<AFGBuildablePowerPole>();
  const bool bSwitch = Host->IsA<AFGBuildableCircuitSwitch>();
  const bool bStorage = Host->IsA<AFGBuildablePowerStorage>();
  // Generators (incl. wind/booster) host a site bus so solo foundation pads work.
  const bool bGenerator = FStructuralEligibilityRules::IsStructuralGenerator(Host);
  if (!bPole && !bSwitch && !bStorage && !bGenerator) {
    return nullptr;
  }

  if (UFGStructuralPowerConnectionComponent* Existing = FindBusConnector(Host)) {
    return Existing;
  }

  UFGStructuralPowerConnectionComponent* Connector =
      NewObject<UFGStructuralPowerConnectionComponent>(
          Host, StructuralPowerConstants::OutletBusConnectorName, RF_Transient);
  if (!Connector) {
    return nullptr;
  }

  Connector->SetMobility(EComponentMobility::Static);
  Host->AddInstanceComponent(Connector);
  Connector->RegisterComponent();
  return Connector;
}

UFGStructuralPowerConnectionComponent* AStructuralPowerGraphSubsystem::GetOrCreatePanelControlBus(
    AFGBuildableLightsControlPanel* Panel) {
  if (!IsValid(Panel)) {
    return nullptr;
  }

  if (UFGStructuralPowerConnectionComponent* Existing = FindPanelControlBus(Panel)) {
    return Existing;
  }

  UFGStructuralPowerConnectionComponent* Connector =
      NewObject<UFGStructuralPowerConnectionComponent>(
          Panel, StructuralPowerConstants::PanelControlBusConnectorName, RF_Transient);
  if (!Connector) {
    return nullptr;
  }

  Connector->SetMobility(EComponentMobility::Static);
  Panel->AddInstanceComponent(Connector);
  Connector->RegisterComponent();
  return Connector;
}

UFGStructuralPowerConnectionComponent* AStructuralPowerGraphSubsystem::GetOrCreateSwitchControlBus(
    AFGBuildableCircuitSwitch* Switch) {
  if (!IsValid(Switch)) {
    return nullptr;
  }

  if (UFGStructuralPowerConnectionComponent* Existing = FindSwitchControlBus(Switch)) {
    return Existing;
  }

  UFGStructuralPowerConnectionComponent* Connector =
      NewObject<UFGStructuralPowerConnectionComponent>(
          Switch, StructuralPowerConstants::SwitchControlBusConnectorName, RF_Transient);
  if (!Connector) {
    return nullptr;
  }

  Connector->SetMobility(EComponentMobility::Static);
  Switch->AddInstanceComponent(Connector);
  Connector->RegisterComponent();
  return Connector;
}

UFGStructuralPowerConnectionComponent*
AStructuralPowerGraphSubsystem::GetOrCreateOutletBusConnector(AFGBuildablePowerPole* Outlet) {
  return GetOrCreateBusConnector(Outlet);
}

FStructuralWallAnchor AStructuralPowerGraphSubsystem::ResolveOutletAnchor(
    AFGBuildable* Outlet) const {
  return const_cast<AStructuralPowerGraphSubsystem*>(this)->BridgeRootIndex.ResolveOutletAnchor(
      Outlet);
}

FStructuralOutletParentResolveParams AStructuralPowerGraphSubsystem::MakeOutletParentResolveParams()
    const {
  return const_cast<AStructuralPowerGraphSubsystem*>(this)
      ->BridgeRootIndex.MakeOutletParentResolveParams();
}

FStructuralComponentResolveResult AStructuralPowerGraphSubsystem::ResolveStructuralComponentAt(
    const FVector& WorldLoc, float QueryRadiusCm, TSubclassOf<AFGBuildable> ClassHint) const {
  return FStructuralAttachmentResolver::ResolveStructuralComponent(StructureGraph, WorldLoc,
                                                                   QueryRadiusCm, ClassHint);
}

FStructuralNodeId AStructuralPowerGraphSubsystem::MakeParentNodeId(
    const FStructuralWallAnchor& Anchor) {
  if (IsValid(Anchor.Actor)) {
    return MakeNodeId(Anchor.Actor);
  }

  if (Anchor.Lightweight.IsValid()) {
    return FStructuralLightweightIndex::MakeNodeId(Anchor.Lightweight);
  }

  return {};
}

void AStructuralPowerGraphSubsystem::OnWorldReady(UWorld* World) {
  GetGraphSession().Bootstrap().OnWorldReady(World);
}

bool AStructuralPowerGraphSubsystem::HasActiveDeferredWork() const {
  return GetPendingJobCount() > 0 || bBulkLoadDrainActive || bPendingPostLoadLightReconcile ||
         bPendingPostLoadMachineReconcile || bPendingFinalLightingReconcile ||
         bPendingStructureSplitReconcile;
}

void AStructuralPowerGraphSubsystem::NotifyDeferredWorkRegistered() {
  if (UWorld* World = GetWorld()) {
    if (FStructuralPowerWorldGate::IsGameplayWorld(World)) {
      UStructuralPowerFactoryTickHandler::RegisterForWorld(World);
    }
  }
}

void AStructuralPowerGraphSubsystem::MaybeReleaseFactoryTick() {
  if (HasActiveDeferredWork()) {
    return;
  }

  if (UWorld* World = GetWorld()) {
    UStructuralPowerFactoryTickHandler::UnregisterForWorld(World);
  }
}

void AStructuralPowerGraphSubsystem::EnqueueLightweightPlacement(
    const FStructuralLightweightKey& Key, bool bDefer) {
  if (!Key.IsValid()) {
    return;
  }

  if (bDefer) {
    if (PlacementQueue.EnqueueLightweight(Key)) {
      NotifyDeferredWorkRegistered();
    }
    return;
  }

  ProcessLightweightStructure(Key);
}

void AStructuralPowerGraphSubsystem::EnqueuePlacement(AFGBuildable* Buildable,
                                                      EStructuralPlacementJobType JobType,
                                                      bool bDefer) {
  if (!IsValid(Buildable)) {
    return;
  }

  if (!FStructuralPowerWorldGate::IsGameplayWorld(Buildable->GetWorld())) {
    return;
  }

  if (!Buildable->HasAuthority()) {
    FStructuralPowerTrace::LogPlacementSkip(Buildable, TEXT("no_authority"));
    return;
  }

  if (bDefer) {
    if (PlacementQueue.EnqueueBuildable(Buildable, JobType)) {
      NotifyDeferredWorkRegistered();
    }
    return;
  }

  if (JobType == EStructuralPlacementJobType::Outlet) {
    ProcessOutlet(Buildable);
  } else if (JobType == EStructuralPlacementJobType::Pipe) {
    ProcessPipe(Buildable);
  } else {
    ProcessStructure(Buildable);
  }
}

bool AStructuralPowerGraphSubsystem::IsBuildablePlacementPending(AFGBuildable* Buildable) const {
  if (!IsValid(Buildable)) {
    return false;
  }

  return PlacementQueue.IsAnyBuildableJobPending(Buildable);
}

void AStructuralPowerGraphSubsystem::TickDeferredPlacements(int32 MaxJobs) {
  PlacementQueue.Tick(
      MaxJobs, bBulkLoadDrainActive,
      [this](AFGBuildable* Buildable, EStructuralPlacementJobType JobType) {
        if (JobType == EStructuralPlacementJobType::Outlet) {
          ProcessOutlet(Buildable);
        } else if (JobType == EStructuralPlacementJobType::Pipe) {
          ProcessPipe(Buildable);
        } else {
          ProcessStructure(Buildable);
        }
      },
      [this](const FStructuralLightweightKey& Key) { ProcessLightweightStructure(Key); },
      [this]() {
        GetGraphSession().Bootstrap().TickLoadPhases();
        MaybeReleaseFactoryTick();
      });
}

void AStructuralPowerGraphSubsystem::TickIdleDeferredWork() {
  GetGraphSession().Bootstrap().TickLoadPhases();
  MaybeRunFinalLightingReconcile();
  MaybeReleaseFactoryTick();
}
