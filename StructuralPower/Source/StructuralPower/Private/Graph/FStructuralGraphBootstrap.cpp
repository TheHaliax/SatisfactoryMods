// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Graph/FStructuralGraphBootstrap.h"

#include "Core/FStructuralGraphSession.h"
#include "Core/FStructuralPowerWorldGate.h"
#include "Graph/FStructuralGraphBulkDrain.h"
#include "Reconcile/FStructuralPowerReconcile.h"
#include "Save/AStructuralPowerGraphSubsystem.h"

#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableLightsControlPanel.h"
#include "Components/UFGStructuralPowerConnectionComponent.h"
#include "Config/FStructuralPowerModConfig.h"
#include "Engine/World.h"
#include "FGBuildableSubsystem.h"
#include "FGLightweightBuildableSubsystem.h"
#include "Lightweight/FStructuralLightweightIndex.h"
#include "Lightweight/FStructuralLightweightTypes.h"
#include "Rules/FStructuralEligibilityRules.h"
#include "Save/FStructuralPlacementQueue.h"
#include "StructuralPowerLog.h"

void FStructuralGraphBootstrap::Bind(FStructuralGraphSession* InSession) {
  Session = InSession;
}

void FStructuralGraphBootstrap::OnWorldReady(UWorld* World) {
  if (!Session || !FStructuralPowerWorldGate::IsGameplayWorld(World) ||
      World->GetNetMode() == NM_Client) {
    return;
  }

  if (Session->PostLoadRebuilt()) {
    return;
  }

  Session->CrossSite().Clear();
  Session->SiteState().ClearFeedSignatures();
  Session->PipeTopology().Reset();

  RebuildBuildableRegistry(World);
  RebuildLightweightIndex(World);
  Session->StructureGraph().Rebuild(World);
  PurgeSavedOutletBusMesh(World);
  Session->PostLoadRebuilt() = true;

  int32 SeededPoles = 0;
  int32 SeededSwitches = 0;
  int32 SeededStorage = 0;
  int32 SeededPanels = 0;
  int32 SeededLights = 0;
  int32 SeededMachines = 0;
  int32 SeededPipeTopo = 0;
  const bool bSeedLighting = FStructuralPowerModConfig::IsGroupLightingEnabled();
  const bool bSeedGeneration = FStructuralPowerModConfig::IsGroupGenerationEnabled();
  const bool bSeedResources = FStructuralPowerModConfig::IsGroupResourcesEnabled();
  const bool bSeedProduction = FStructuralPowerModConfig::IsGroupProductionEnabled();
  const bool bSeedTransport = FStructuralPowerModConfig::IsGroupTransportEnabled();
  const bool bSeedPipes = FStructuralPowerModConfig::IsGroupPipesEnabled();
  if (AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(World)) {
    for (AFGBuildable* Buildable : BuildableSubsystem->GetAllBuildablesRef()) {
      if (!IsValid(Buildable) || !Buildable->HasAuthority()) {
        continue;
      }
      if (FStructuralEligibilityRules::IsFluidPipeSupport(Buildable) ||
          FStructuralEligibilityRules::IsFluidPipeConductor(Buildable)) {
        Session->EnqueuePlacement(Buildable, EStructuralPlacementJobType::Pipe, /*bDefer=*/true);
        ++SeededPipeTopo;
      }
    }

    for (AFGBuildable* Buildable : BuildableSubsystem->GetAllBuildablesRef()) {
      if (!IsValid(Buildable) || !Buildable->HasAuthority()) {
        continue;
      }

      if (FStructuralEligibilityRules::IsPowerBridgePole(Buildable)) {
        Session->EnqueuePlacement(Buildable, EStructuralPlacementJobType::Outlet, /*bDefer=*/true);
        ++SeededPoles;
      } else if (FStructuralEligibilityRules::IsPowerBridgeSwitch(Buildable)) {
        Session->EnqueuePlacement(Buildable, EStructuralPlacementJobType::Outlet, /*bDefer=*/true);
        ++SeededSwitches;
      } else if (FStructuralEligibilityRules::IsPowerStorage(Buildable)) {
        Session->EnqueuePlacement(Buildable, EStructuralPlacementJobType::Outlet, /*bDefer=*/true);
        ++SeededStorage;
      } else if (bSeedLighting && Buildable->IsA<AFGBuildableLightsControlPanel>()) {
        Session->EnqueuePlacement(Buildable, EStructuralPlacementJobType::Outlet, /*bDefer=*/true);
        ++SeededPanels;
      } else if (bSeedLighting &&
                 FStructuralEligibilityRules::IsStructuralLightConsumer(Buildable)) {
        Session->EnqueuePlacement(Buildable, EStructuralPlacementJobType::Outlet, /*bDefer=*/true);
        ++SeededLights;
      } else if (bSeedGeneration && FStructuralEligibilityRules::IsStructuralGenerator(Buildable)) {
        Session->EnqueuePlacement(Buildable, EStructuralPlacementJobType::Outlet, /*bDefer=*/true);
        ++SeededMachines;
      } else if (bSeedResources && FStructuralEligibilityRules::IsStructuralExtractor(Buildable)) {
        Session->EnqueuePlacement(Buildable, EStructuralPlacementJobType::Outlet, /*bDefer=*/true);
        ++SeededMachines;
      } else if (bSeedProduction &&
                 FStructuralEligibilityRules::IsStructuralManufacturer(Buildable)) {
        Session->EnqueuePlacement(Buildable, EStructuralPlacementJobType::Outlet, /*bDefer=*/true);
        ++SeededMachines;
      } else if (bSeedTransport && FStructuralEligibilityRules::IsStructuralTransport(Buildable)) {
        Session->EnqueuePlacement(Buildable, EStructuralPlacementJobType::Outlet, /*bDefer=*/true);
        ++SeededMachines;
      } else if (bSeedPipes && FStructuralEligibilityRules::IsStructuralPipelinePump(Buildable)) {
        Session->EnqueuePlacement(Buildable, EStructuralPlacementJobType::Outlet, /*bDefer=*/true);
        ++SeededMachines;
      }
    }
  }

  int32 Components = 0;
  int32 Largest = 0;
  Session->StructureGraph().GetComponentStats(Components, Largest);

  UE_LOG(LogStructuralPower, Log,
         TEXT("Graph ready — %d structure node(s), %d component(s) (largest %d), %d LW tracked,"
              " %d pole(s) seeded, %d switch(es) seeded, %d storage seeded,"
              " %d panel(s) seeded, %d light(s) seeded, %d machine(s) seeded,"
              " %d pipe topo seeded"),
         Session->StructureGraph().GetNodeCount(), Components, Largest,
         Session->LightweightIndex().GetTrackedCount(), SeededPoles, SeededSwitches, SeededStorage,
         SeededPanels, SeededLights, SeededMachines, SeededPipeTopo);

  Session->BulkLoadDrainActive() = (SeededPoles + SeededSwitches + SeededStorage + SeededPanels +
                                    SeededLights + SeededMachines + SeededPipeTopo) > 0;
  if (Session->BulkLoadDrainActive()) {
    Session->EndpointIndex().Reset();
    Session->BridgeEndpointRootIndexDirty() = false;
  } else {
    Session->BridgeRootIndex().MarkBridgeEndpointRootIndexDirty();
  }

  if (SeededMachines > 0) {
    Session->PendingPostLoadMachineReconcile() = true;
  }

  if (FStructuralPowerModConfig::IsGroupLightingEnabled()) {
    Session->PendingPostLoadLightReconcile() = true;
  }

  if (Session->HasActiveDeferredWork()) {
    Session->NotifyDeferredWorkRegistered();
  }
}

void FStructuralGraphBootstrap::TickLoadPhases() {
  if (!Session) {
    return;
  }

  if (Session->GetPendingJobCount() > 0) {
    return;
  }

  if (Session->BulkLoadDrainActive()) {
    Session->BulkDrain().FinishBulkLoadDrain();
    if (Session->HasPendingBulkRemesh()) {
      return;
    }
  }

  if (Session->PendingPostLoadMachineReconcile()) {
    Session->BulkLoadDrainActive() = false;
    Session->Reconcile().RunPostLoadMachineWorkers();
    Session->PendingPostLoadMachineReconcile() = false;
  }

  if (!Session->PendingPostLoadLightReconcile()) {
    Session->BulkLoadDrainActive() = false;
    Session->MaybeReleaseFactoryTick();
    return;
  }

  Session->BulkLoadDrainActive() = false;
  Session->Reconcile().RunPostLoadLightWorkers();
  Session->PendingPostLoadLightReconcile() = false;
  Session->MaybeReleaseFactoryTick();
}

void FStructuralGraphBootstrap::PurgeSavedOutletBusMesh(UWorld* World) {
  AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(World);
  if (!IsValid(BuildableSubsystem)) {
    return;
  }

  int32 Purged = 0;

  for (AFGBuildable* Buildable : BuildableSubsystem->GetAllBuildablesRef()) {
    if (!IsValid(Buildable) || !Buildable->HasAuthority()) {
      continue;
    }

    const bool bPole = FStructuralEligibilityRules::IsPowerBridgePole(Buildable);
    const bool bSwitch = FStructuralEligibilityRules::IsPowerBridgeSwitch(Buildable);
    const bool bStorage = FStructuralEligibilityRules::IsPowerStorage(Buildable);
    const bool bPanel = FStructuralPowerModConfig::IsGroupLightingEnabled() &&
                        Buildable->IsA<AFGBuildableLightsControlPanel>();
    if (!bPole && !bSwitch && !bStorage && !bPanel) {
      continue;
    }

    const bool bHasModBus =
        IsValid(Session->FindBusConnector(Buildable)) ||
        (bSwitch && IsValid(AStructuralPowerGraphSubsystem::FindSwitchControlBus(Buildable))) ||
        (bPanel && IsValid(AStructuralPowerGraphSubsystem::FindPanelControlBus(Buildable)));
    if (!bHasModBus) {
      continue;
    }

    AStructuralPowerGraphSubsystem::StripPersistedEndpointModComponents(Buildable);
    ++Purged;
  }

  if (Purged > 0) {
    UE_LOG(LogStructuralPower, Log,
           TEXT("Purged saved mod bus mesh on %d endpoint(s) — rebuilding from geometry"), Purged);
  }
}

void FStructuralGraphBootstrap::RebuildBuildableRegistry(UWorld* World) {
  Session->RegisteredBuildables().Reset();
  Session->BusMemberSpatialIndex().Reset();

  if (!IsValid(World)) {
    return;
  }

  Session->BusMemberSpatialIndex().RebuildFromWorld(World);

  AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(World);
  if (!IsValid(BuildableSubsystem)) {
    return;
  }

  for (AFGBuildable* Buildable : BuildableSubsystem->GetAllBuildablesRef()) {
    if (!IsValid(Buildable)) {
      continue;
    }

    if (FStructuralEligibilityRules::IsBusMember(Buildable) ||
        FStructuralEligibilityRules::IsPowerBridgePole(Buildable) ||
        FStructuralEligibilityRules::IsPowerBridgeSwitch(Buildable)) {
      Session->RegisteredBuildables().Add(Session->MakeNodeId(Buildable), Buildable);
    }
  }
}

void FStructuralGraphBootstrap::RebuildLightweightIndex(UWorld* World) {
  if (!IsValid(World)) {
    return;
  }

  AFGLightweightBuildableSubsystem* Lw = AFGLightweightBuildableSubsystem::Get(World);
  if (!IsValid(Lw)) {
    return;
  }

  for (const TPair<TSubclassOf<AFGBuildable>, TArray<FRuntimeBuildableInstanceData>>& Pair :
       Lw->GetAllLightweightBuildableInstances()) {
    TSubclassOf<AFGBuildable> Class = Pair.Key;
    if (!Class) {
      continue;
    }

    const AFGBuildable* CDO = Class->GetDefaultObject<AFGBuildable>();
    if (!FStructuralEligibilityRules::IsBusMember(CDO)) {
      continue;
    }

    const TArray<FRuntimeBuildableInstanceData>& Instances = Pair.Value;
    for (int32 Index = 0; Index < Instances.Num(); ++Index) {
      if (Instances[Index].IsValidOnLoad()) {
        Session->LightweightIndex().RegisterTrackedMember(World,
                                                          FStructuralLightweightKey{Class, Index});
      }
    }
  }
}
