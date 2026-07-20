// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Attach/FStructuralSwitchBridgeStrategy.h"

#include "Attach/FStructuralDeviceAttach.h"
#include "Attach/FStructuralSwitchPredicates.h"
#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Circuit/FStructuralCircuitPromotionUtil.h"
#include "Circuit/FStructuralGraphCircuitOps.h"
#include "Components/UFGStructuralPowerConnectionComponent.h"
#include "Core/FStructuralGraphSession.h"
#include "Core/FStructuralPowerContext.h"
#include "Diagnostics/FStructuralPowerTraceScope.h"
#include "FGCircuitConnectionComponent.h"
#include "FGPowerConnectionComponent.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Graph/FStructuralSwitchParentResolver.h"
#include "Processors/FStructuralPowerSwitchProcessor.h"
#include "Save/AStructuralPowerGraphSubsystem.h"

namespace {
static void StrategyOpenDirectedBridgeLeg(UFGStructuralPowerConnectionComponent* SourceBus,
                                          UFGCircuitConnectionComponent* Leg) {
  if (!IsValid(SourceBus) || !IsValid(Leg) || SourceBus == Leg) {
    return;
  }

  if (SourceBus->HasHiddenConnection(Leg)) {
    SourceBus->RemoveHiddenConnection(Leg);
  }
}

static void StrategyCloseDirectedBridgeLeg(UFGStructuralPowerConnectionComponent* SourceBus,
                                           UFGPowerConnectionComponent* Leg) {
  if (!IsValid(SourceBus) || !IsValid(Leg) || SourceBus == Leg) {
    return;
  }

  if (!SourceBus->HasHiddenConnection(Leg)) {
    SourceBus->AddHiddenConnection(Leg);
  }
}

static UFGPowerConnectionComponent*
FindSingleWireSourceBridgeLeg(AFGBuildableCircuitSwitch* Switch) {
  if (!IsValid(Switch)) {
    return nullptr;
  }

  UFGPowerConnectionComponent* Conn0Power =
      Cast<UFGPowerConnectionComponent>(Switch->GetConnection0());
  UFGPowerConnectionComponent* Conn1Power =
      Cast<UFGPowerConnectionComponent>(Switch->GetConnection1());
  const bool bConn0Wired = IsValid(Conn0Power) && Conn0Power->GetNumConnections() > 0;
  const bool bConn1Wired = IsValid(Conn1Power) && Conn1Power->GetNumConnections() > 0;

  if (bConn0Wired && !bConn1Wired && IsValid(Conn1Power)) {
    return Conn1Power;
  }

  if (bConn1Wired && !bConn0Wired && IsValid(Conn0Power)) {
    return Conn0Power;
  }

  return nullptr;
}

static void AssignConfiguredBridgeSlots(UFGStructuralPowerConnectionComponent* SourceBus,
                                        UFGStructuralPowerConnectionComponent* ControlBus,
                                        AFGBuildableCircuitSwitch* Switch) {
  if (!IsValid(SourceBus) || !IsValid(ControlBus) || !IsValid(Switch)) {
    return;
  }

  StrategyOpenDirectedBridgeLeg(SourceBus, ControlBus);
  StrategyOpenDirectedBridgeLeg(SourceBus, Switch->GetConnection0());
  StrategyOpenDirectedBridgeLeg(SourceBus, Switch->GetConnection1());
  StrategyOpenDirectedBridgeLeg(ControlBus, Switch->GetConnection0());
  StrategyOpenDirectedBridgeLeg(ControlBus, Switch->GetConnection1());

  if (UFGPowerConnectionComponent* Conn0 =
          Cast<UFGPowerConnectionComponent>(Switch->GetConnection0())) {
    StrategyCloseDirectedBridgeLeg(SourceBus, Conn0);
  }

  if (UFGPowerConnectionComponent* Conn1 =
          Cast<UFGPowerConnectionComponent>(Switch->GetConnection1())) {
    StrategyCloseDirectedBridgeLeg(ControlBus, Conn1);
  }
}

static void OpenConfiguredBridgeSlots(UFGStructuralPowerConnectionComponent* SourceBus,
                                      UFGStructuralPowerConnectionComponent* ControlBus,
                                      AFGBuildableCircuitSwitch* Switch) {
  if (!IsValid(Switch)) {
    return;
  }

  if (IsValid(SourceBus)) {
    StrategyOpenDirectedBridgeLeg(SourceBus, Switch->GetConnection0());
    StrategyOpenDirectedBridgeLeg(SourceBus, Switch->GetConnection1());
  }

  if (IsValid(ControlBus)) {
    StrategyOpenDirectedBridgeLeg(ControlBus, Switch->GetConnection0());
    StrategyOpenDirectedBridgeLeg(ControlBus, Switch->GetConnection1());
  }

  if (IsValid(SourceBus) && IsValid(ControlBus)) {
    StrategyOpenDirectedBridgeLeg(SourceBus, ControlBus);
  }
}

static void StrategyCloseBypassZeroWireOnLegs(UFGStructuralPowerConnectionComponent* SourceBus,
                                              AFGBuildableCircuitSwitch* Switch) {
  StrategyOpenDirectedBridgeLeg(SourceBus, Switch->GetConnection0());
  StrategyOpenDirectedBridgeLeg(SourceBus, Switch->GetConnection1());

  if (UFGPowerConnectionComponent* Conn0 =
          Cast<UFGPowerConnectionComponent>(Switch->GetConnection0())) {
    StrategyCloseDirectedBridgeLeg(SourceBus, Conn0);
  }

  if (UFGPowerConnectionComponent* Conn1 =
          Cast<UFGPowerConnectionComponent>(Switch->GetConnection1())) {
    StrategyCloseDirectedBridgeLeg(SourceBus, Conn1);
  }
}

static int32 StrategyCountVanillaWirePorts(AFGBuildableCircuitSwitch* Switch) {
  int32 Count = 0;
  if (const UFGPowerConnectionComponent* Conn0 =
          Cast<UFGPowerConnectionComponent>(Switch->GetConnection0())) {
    if (Conn0->GetNumConnections() > 0) {
      ++Count;
    }
  }

  if (const UFGPowerConnectionComponent* Conn1 =
          Cast<UFGPowerConnectionComponent>(Switch->GetConnection1())) {
    if (Conn1->GetNumConnections() > 0) {
      ++Count;
    }
  }

  return Count;
}
} // namespace

void FStructuralSwitchBridgeStrategy::DisarmDirectedPair(FStructuralPowerContext& Ctx,
                                                         AFGBuildableCircuitSwitch* Switch) {
  if (!IsValid(Switch)) {
    return;
  }

  UFGStructuralPowerConnectionComponent* SourceBus = Ctx.Session().FindBusConnector(Switch);
  if (!IsValid(SourceBus)) {
    return;
  }

  if (UFGPowerConnectionComponent* UnwiredLeg = FindSingleWireSourceBridgeLeg(Switch)) {
    StrategyOpenDirectedBridgeLeg(SourceBus, UnwiredLeg);
  }

  StrategyOpenDirectedBridgeLeg(SourceBus, Switch->GetConnection0());
  StrategyOpenDirectedBridgeLeg(SourceBus, Switch->GetConnection1());

  if (UFGStructuralPowerConnectionComponent* ControlBus =
          AStructuralPowerGraphSubsystem::FindSwitchControlBus(Switch)) {
    FStructuralDeviceAttach::TearDownConsumerLinks(ControlBus);
    OpenConfiguredBridgeSlots(SourceBus, ControlBus, Switch);
  }
}

void FStructuralSwitchBridgeStrategy::SyncDirectedBridgePair(FStructuralPowerContext& Ctx,
                                                             AFGBuildableCircuitSwitch* Switch) {
  if (!IsValid(Switch)) {
    return;
  }

  const bool bWired = FStructuralSwitchParentResolver::HasAnyVanillaWire(Switch);
  const bool bConfigured = FStructuralSwitchPredicates::HasAssignedControl(Ctx, Switch);
  if (!bWired && !bConfigured) {
    return;
  }

  UFGStructuralPowerConnectionComponent* SourceBus = Ctx.Session().FindBusConnector(Switch);
  if (!IsValid(SourceBus)) {
    SourceBus = Ctx.Session().GetOrCreateBusConnector(Switch);
  }
  if (!IsValid(SourceBus)) {
    return;
  }

  if (bWired) {
    StrategyOpenDirectedBridgeLeg(SourceBus, Switch->GetConnection0());
    StrategyOpenDirectedBridgeLeg(SourceBus, Switch->GetConnection1());
    if (UFGPowerConnectionComponent* SourceBridgeLeg = FindSingleWireSourceBridgeLeg(Switch)) {
      StrategyCloseDirectedBridgeLeg(SourceBus, SourceBridgeLeg);
    }

    if (UFGStructuralPowerConnectionComponent* ControlBus =
            AStructuralPowerGraphSubsystem::FindSwitchControlBus(Switch)) {
      FStructuralDeviceAttach::TearDownConsumerLinks(ControlBus);
      OpenConfiguredBridgeSlots(SourceBus, ControlBus, Switch);
    }
  } else if (bConfigured) {
    UFGStructuralPowerConnectionComponent* ControlBus =
        Ctx.Session().GetOrCreateSwitchControlBus(Switch);
    if (IsValid(ControlBus)) {
      AssignConfiguredBridgeSlots(SourceBus, ControlBus, Switch);
    }
  }
}

void FStructuralSwitchBridgeStrategy::ApplyMembership(FStructuralPowerContext& Ctx,
                                                      AFGBuildableCircuitSwitch* Switch) {
  if (!IsValid(Switch)) {
    return;
  }

  const bool bWired = FStructuralSwitchParentResolver::HasAnyVanillaWire(Switch);
  const bool bConfigured = FStructuralSwitchPredicates::HasAssignedControl(Ctx, Switch);
  const bool bSwitchOn = Switch->IsBridgeActive();
  const int32 WirePortCount = StrategyCountVanillaWirePorts(Switch);

  FStructuralGraphSession& Session = Ctx.Session();
  UFGStructuralPowerConnectionComponent* SourceBus = Session.FindBusConnector(Switch);
  if (!IsValid(SourceBus)) {
    SourceBus = Session.GetOrCreateBusConnector(Switch);
  }

  if (!IsValid(SourceBus)) {
    return;
  }

  if (!bSwitchOn) {
    DisarmDirectedPair(Ctx, Switch);
    return;
  }

  if (bConfigured || bWired) {
    const bool bMeshOnlyLinks = WirePortCount >= 2;
    Session.Circuit().LinkBusToVisibleConnectionsLocal(Switch, SourceBus, bMeshOnlyLinks);
    SyncDirectedBridgePair(Ctx, Switch);

    if (bWired && WirePortCount < 2) {
      Session.Circuit().PromoteOutletBusIfPowered(SourceBus, /*bLocalPromoteOnly=*/true);
    }

    return;
  }

  Session.Circuit().LinkBusToVisibleConnectionsLocal(Switch, SourceBus, /*bMeshOnlyLinks=*/true);
  StrategyCloseBypassZeroWireOnLegs(SourceBus, Switch);
}

void FStructuralSwitchBridgeStrategy::ApplyToggle(FStructuralPowerContext& Ctx,
                                                  AFGBuildableCircuitSwitch* Switch) {
  if (!IsValid(Switch)) {
    return;
  }

  if (!Switch->IsBridgeActive()) {
    DisarmDirectedPair(Ctx, Switch);
    return;
  }

  const bool bWired = FStructuralSwitchParentResolver::HasAnyVanillaWire(Switch);
  const bool bConfigured = FStructuralSwitchPredicates::HasAssignedControl(Ctx, Switch);
  if (bWired || bConfigured) {
    SyncDirectedBridgePair(Ctx, Switch);
    return;
  }

  FStructuralGraphSession& Session = Ctx.Session();
  UFGStructuralPowerConnectionComponent* SourceBus = Session.FindBusConnector(Switch);
  if (!IsValid(SourceBus)) {
    SourceBus = Session.GetOrCreateBusConnector(Switch);
  }

  if (!IsValid(SourceBus)) {
    return;
  }

  Session.Circuit().LinkBusToVisibleConnectionsLocal(Switch, SourceBus, /*bMeshOnlyLinks=*/true);
  StrategyCloseBypassZeroWireOnLegs(SourceBus, Switch);
}

void FStructuralSwitchBridgeStrategy::ApplyWireEcho(FStructuralPowerContext& Ctx,
                                                    AFGBuildableCircuitSwitch* Switch) {
  HALSP_TRACE_SCOPE_DETAIL(TEXT("mod"), TEXT("wireDelta.switch"),
                           IsValid(Switch) ? Switch->GetName() : TEXT("null"));

  if (!IsValid(Switch)) {
    return;
  }

  FStructuralGraphSession& Session = Ctx.Session();
  UFGStructuralPowerConnectionComponent* OutletBus = Session.FindBusConnector(Switch);
  if (!IsValid(OutletBus)) {
    OutletBus = Session.GetOrCreateBusConnector(Switch);
  }

  if (!IsValid(OutletBus)) {
    return;
  }

  if (!Session.Circuit().HasBridgeBusPeerMesh(OutletBus)) {
    FStructuralPowerSwitchProcessor::ApplyStructureMembership(Ctx, Switch);
    return;
  }

  Session.Circuit().LinkBusToVisibleConnectionsLocal(Switch, OutletBus, /*bMeshOnlyLinks=*/true);

  if (FStructuralCircuitPromotionUtil::ComponentOnCircuit(OutletBus)) {
    Session.Circuit().PromoteDirectHiddenLinks(OutletBus);
  }

  if (Switch->IsBridgeActive()) {
    SyncDirectedBridgePair(Ctx, Switch);
  } else {
    DisarmDirectedPair(Ctx, Switch);
  }
}

void FStructuralSwitchBridgeStrategy::Apply(FStructuralPowerContext& Ctx,
                                            AFGBuildableCircuitSwitch* Switch) {
  ApplyMembership(Ctx, Switch);
}

void FStructuralSwitchBridgeStrategy::RemeshUnmeshedBridgesAfterBulkLoad(
    FStructuralPowerContext& Ctx) {
  FStructuralGraphSession& Session = Ctx.Session();
  Session.BridgeRootIndex().RefreshBridgeEndpointRootIndex();

  TArray<const TPair<FStructuralNodeId, FTrackedEndpoint>*> Ordered;
  Ordered.Reserve(Session.TrackedEndpoints().Num());
  for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : Session.TrackedEndpoints()) {
    const EStructuralEndpointKind Kind = Pair.Value.Kind;
    if (Kind != EStructuralEndpointKind::Pole && Kind != EStructuralEndpointKind::Switch &&
        Kind != EStructuralEndpointKind::Storage && Kind != EStructuralEndpointKind::Generator) {
      continue;
    }

    if (Kind == EStructuralEndpointKind::Pole && !Pair.Value.bStructuralPowerTransferActive) {
      continue;
    }

    Ordered.Add(&Pair);
  }

  Ordered.Sort([](const TPair<FStructuralNodeId, FTrackedEndpoint>& A,
                  const TPair<FStructuralNodeId, FTrackedEndpoint>& B) {
    auto Rank = [](EStructuralEndpointKind Kind) -> int32 {
      if (Kind == EStructuralEndpointKind::Pole) {
        return 0;
      }
      if (Kind == EStructuralEndpointKind::Storage || Kind == EStructuralEndpointKind::Generator) {
        return 1;
      }
      return 2;
    };
    return Rank(A.Value.Kind) < Rank(B.Value.Kind);
  });

  TMap<int32, FStructuralNodeId> HubByRoot;

  for (const TPair<FStructuralNodeId, FTrackedEndpoint>* PairPtr : Ordered) {
    const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair = *PairPtr;
    const EStructuralEndpointKind Kind = Pair.Value.Kind;

    AFGBuildable* Host = Pair.Value.Actor.Get();
    if (!IsValid(Host) || !Pair.Value.MountParentId.IsValid()) {
      continue;
    }

    const int32 Root = Session.StructureGraph().FindRoot(Pair.Value.MountParentId);
    if (Root == INDEX_NONE) {
      continue;
    }

    UFGStructuralPowerConnectionComponent* OutletBus = Session.GetOrCreateBusConnector(Host);
    if (!IsValid(OutletBus)) {
      continue;
    }

    if (Kind != EStructuralEndpointKind::Switch) {
      Session.Circuit().LinkBusToVisibleConnectionsLocal(Host, OutletBus, /*bMeshOnlyLinks=*/false);
    }

    if (const FStructuralNodeId* HubId = HubByRoot.Find(Root)) {
      if (*HubId != Pair.Key) {
        const FTrackedEndpoint* HubTracked = Session.TrackedEndpoints().Find(*HubId);
        AFGBuildable* HubHost = HubTracked ? HubTracked->Actor.Get() : nullptr;
        if (UFGStructuralPowerConnectionComponent* HubBus = Session.FindBusConnector(HubHost)) {
          if (!OutletBus->HasHiddenConnection(HubBus)) {
            Session.Circuit().LinkHiddenPairLocal(OutletBus, HubBus, /*bPromoteCircuit=*/true);
          }
        }
      }
    } else {
      HubByRoot.Add(Root, Pair.Key);
    }

    if (Kind != EStructuralEndpointKind::Switch) {
      Session.Circuit().PromoteOutletBusIfPowered(OutletBus, /*bLocalPromoteOnly=*/false);
    }
  }
}
