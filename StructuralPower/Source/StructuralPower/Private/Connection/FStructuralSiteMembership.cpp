// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Connection/FStructuralSiteMembership.h"

#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "Circuit/FStructuralCircuitPromotionUtil.h"
#include "Circuit/FStructuralGraphCircuitOps.h"
#include "Components/UFGStructuralPowerConnectionComponent.h"
#include "Core/FStructuralGraphSession.h"
#include "Diagnostics/FStructuralPowerTrace.h"
#include "Diagnostics/FStructuralPowerTraceScope.h"
#include "FGCircuitConnectionComponent.h"
#include "Graph/FStructuralBridgeRootIndex.h"
#include "Graph/FStructuralConnectivityGraph.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Graph/FStructuralHostAttachAdapter.h"
#include "Graph/FStructuralOutletParentHeuristics.h"
#include "Graph/FStructuralOutletParentResolver.h"
#include "Graph/FStructuralSwitchParentResolver.h"
#include "Routing/EStructuralChannel.h"
#include "Save/FStructuralEndpointIdRegistry.h"
#include "Save/FStructuralGraphIdOps.h"
#include "Save/FStructuralPlacementQueue.h"
#include "StructuralPowerLog.h"

namespace {
static void StripSwitchVanillaPortHiddenLinks(AFGBuildableCircuitSwitch* Switch,
                                              UFGStructuralPowerConnectionComponent* Bus) {
  if (!IsValid(Switch) || !IsValid(Bus)) {
    return;
  }

  auto OpenLeg = [&](UFGCircuitConnectionComponent* Leg) {
    if (!IsValid(Leg) || Bus == Leg) {
      return;
    }

    if (Bus->HasHiddenConnection(Leg)) {
      Bus->RemoveHiddenConnection(Leg);
    }
  };

  OpenLeg(Switch->GetConnection0());
  OpenLeg(Switch->GetConnection1());
}
}  // namespace

bool FStructuralSiteMembership::ResolveSiteContext(FStructuralGraphSession& Session,
                                                   AFGBuildable* Endpoint,
                                                   FStructuralSiteContext& OutSite,
                                                   bool bUsePoleRootResolver) {
  OutSite = FStructuralSiteContext{};
  if (!IsValid(Endpoint)) {
    return false;
  }

  const FStructuralOutletParentResolveParams Params =
      Session.BridgeRootIndex().MakeOutletParentResolveParams();
  const FStructuralOutletParentResolveResult ParentResolve =
      FStructuralOutletParentResolver::ResolveDetailed(Endpoint, Session.GetWorld(), Params);

  OutSite.ParentAnchor = ParentResolve.Anchor;
  OutSite.ParentMethod = ParentResolve.Method;
  OutSite.HostKind = FStructuralHostAttachAdapter::ClassifyHost(ParentResolve.Anchor);
  OutSite.bAnchored = FStructuralHostAttachAdapter::ConfirmSiteAttach(ParentResolve, Endpoint);

  if (!OutSite.bAnchored) {
    return false;
  }

  if (bUsePoleRootResolver) {
    AFGBuildablePowerPole* Pole = Cast<AFGBuildablePowerPole>(Endpoint);
    if (IsValid(Pole)) {
      OutSite.SiteRoot = Session.BridgeRootIndex().ResolvePoleComponentRoot(
          Pole, OutSite.ParentAnchor, OutSite.MountParentId);
    }
  } else {
    OutSite.SiteRoot = Session.BridgeRootIndex().ResolveEndpointComponentRoot(
        Endpoint, OutSite.ParentAnchor, OutSite.MountParentId);
  }

  return OutSite.SiteRoot != INDEX_NONE;
}

void FStructuralSiteMembership::RegisterOnBulkLoad(FStructuralGraphSession& Session,
                                                   AFGBuildable* Host, EStructuralEndpointKind Kind,
                                                   FTrackedEndpoint& Tracked,
                                                   const FStructuralSiteContext& Site) {
  if (!IsValid(Host)) {
    return;
  }

  Tracked.Actor = Host;
  Tracked.Kind = Kind;
  Tracked.MountParentId = Site.MountParentId;
  Tracked.bAwaitingStructuralSite = false;
  Session.RegisterBuildableActor(Host);

  if (Kind != EStructuralEndpointKind::Switch) {
    Tracked.bStructuralPowerTransferActive = Site.bAnchored && Site.SiteRoot != INDEX_NONE;
  }
}

void FStructuralSiteMembership::IntegrateOnPlace(
    FStructuralGraphSession& Session, AFGBuildable* Host,
    UFGStructuralPowerConnectionComponent* OutletBus, const FStructuralNodeId& EndpointId,
    EStructuralEndpointKind Kind, FTrackedEndpoint& Tracked, const FStructuralSiteContext& Site,
    const FStructuralSiteMembershipParams& Params) {
  HALSP_TRACE_SCOPE_DETAIL(TEXT("mod"), TEXT("site.IntegrateOnPlace"),
                           IsValid(Host) ? Host->GetName() : TEXT("null"));

  if (!IsValid(Host) || !IsValid(OutletBus)) {
    return;
  }

  Tracked.Actor = Host;
  Tracked.Kind = Kind;
  Tracked.MountParentId = Site.MountParentId;
  Tracked.bAwaitingStructuralSite = !(Site.bAnchored && Site.SiteRoot != INDEX_NONE);
  Session.RegisterBuildableActor(Host);
  if (!Params.bSkipEndpointIndexDirty) {
    Session.BridgeRootIndex().MarkBridgeEndpointRootIndexDirty();
  }

  if (Kind != EStructuralEndpointKind::Switch) {
    Tracked.bStructuralPowerTransferActive = Site.bAnchored && Site.SiteRoot != INDEX_NONE;
  }

  if (Params.bStripSwitchVanillaPortLinks) {
    if (AFGBuildableCircuitSwitch* Switch = Cast<AFGBuildableCircuitSwitch>(Host)) {
      StripSwitchVanillaPortHiddenLinks(Switch, OutletBus);
    }
  }

  if (Params.bLinkVisibleConnections) {
    Session.Circuit().LinkBusToVisibleConnectionsLocal(Host, OutletBus, Params.bMeshOnlyLinks);
  }

  if (Site.bAnchored && Site.SiteRoot != INDEX_NONE &&
      !Session.Circuit().HasBridgeBusPeerMesh(OutletBus) && !Session.IsBulkLoadDrainActive()) {
    Session.Circuit().TryMeshPeerBusOnComponent(Host, OutletBus, Site.SiteRoot, EndpointId,
                                                Params.bBridgePeersOnly, Params.bMeshOnlyLinks);
  }

  if (Site.bAnchored && !Params.bMeshOnlyLinks) {
    Session.Circuit().PromoteOutletBusIfPowered(OutletBus, /*bLocalPromoteOnly=*/true);
  }

  const FVector AnchorLocation = FStructuralOutletParentHeuristics::GetOutletAnchorLocation(Host);
  const float ParentDistCm = Site.ParentAnchor.IsValid()
                                 ? FVector::Dist(AnchorLocation, Site.ParentAnchor.WorldLocation)
                                 : -1.0f;
  if (!Session.IsBulkLoadDrainActive()) {
    FName StructureName = NAME_None;
    const FStructuralComponentKey CompKey = Session.Ids().MakeComponentKeyForRoot(Site.SiteRoot);
    if (CompKey.IsValid()) {
      Session.IdRegistry().TryGetComponentDefaultId(CompKey, StructureName);
    }
    const FString MountLabel =
        Site.MountParentId.IsLightweight()
            ? FString::Printf(TEXT("LW%d"), Site.MountParentId.LightweightIndex)
            : Site.MountParentId.ActorName.ToString();

    UE_LOG(LogStructuralPower, Log,
           TEXT("[HALSP] site integrate %s kind=%s structure=%s site=%d mount=%s host=%d"
                " anchored=%d parentMethod=%s distCm=%.0f busCircuit=%d powered=%d"),
           *Host->GetName(), StructuralEndpointKindToString(Kind),
           StructureName.IsNone() ? TEXT("-") : *StructureName.ToString(), Site.SiteRoot,
           *MountLabel, static_cast<int32>(Site.HostKind), Site.bAnchored ? 1 : 0,
           FStructuralOutletParentResolver::FormatParentMethod(Site.ParentMethod), ParentDistCm,
           OutletBus->GetCircuitID(),
           FStructuralCircuitPromotionUtil::ComponentCarriesPower(OutletBus) ? 1 : 0);
  }
}

FStructuralSwitchMembershipResult FStructuralSiteMembership::IntegrateSwitchOnPlace(
    FStructuralGraphSession& Session, AFGBuildableCircuitSwitch* Switch,
    const FStructuralSwitchMembershipParams& Params) {
  FStructuralSwitchMembershipResult Result;
  if (!IsValid(Switch)) {
    Result.bDone = true;
    return Result;
  }

  const bool bBulk = Session.IsBulkLoadDrainActive();
  const bool bFactRefresh = Params.AttachContext == EAttachContext::WireDelta;
  const FStructuralNodeId SwitchId = Session.MakeNodeId(Switch);

  FTrackedEndpoint& Tracked = Session.TrackedEndpoints().FindOrAdd(SwitchId);
  FStructuralSiteContext& Site = Result.Site;
  if (!ResolveSiteContext(Session, Switch, Site)) {
    const FStructuralOutletParentResolveParams ParentParams =
        Session.BridgeRootIndex().MakeOutletParentResolveParams();
    const FStructuralSwitchParentResolveResult ParentResolve =
        FStructuralSwitchParentResolver::Resolve(
            Switch, Session.GetWorld(), Session.StructureGraph(), Session.LightweightIndex(),
            /*bPreferWirePort=*/false, &ParentParams);
    Site.ParentAnchor = ParentResolve.Anchor;
    Session.BridgeRootIndex().ResolveEndpointComponentRoot(Switch, Site.ParentAnchor,
                                                           Site.MountParentId);
    Tracked.MountParentId = Site.MountParentId;
    Site.SiteRoot = Session.BridgeRootIndex().FindRootForTrackedEndpoint(Tracked);
    Site.bAnchored = Site.SiteRoot != INDEX_NONE;
  }

  const int32 Root = Site.SiteRoot;
  Tracked.Actor = Switch;
  Tracked.Kind = EStructuralEndpointKind::Switch;
  if (Site.MountParentId.IsValid()) {
    Tracked.MountParentId = Site.MountParentId;
  }

  const bool bWired = FStructuralSwitchParentResolver::HasAnyVanillaWire(Switch);

  if (bBulk) {
    if (Site.bAnchored && Root != INDEX_NONE) {
      RegisterOnBulkLoad(Session, Switch, EStructuralEndpointKind::Switch, Tracked, Site);
      if (Site.MountParentId.IsValid()) {
        Session.BridgeRootIndex().AddEndpointToRootIndex(Root, EStructuralEndpointKind::Switch,
                                                         SwitchId);
      }
    } else {
      Tracked.bAwaitingStructuralSite = true;
      Session.RegisterBuildableActor(Switch);
    }

    Result.bDone = true;
    return Result;
  }

  UFGStructuralPowerConnectionComponent* OutletBus = Session.GetOrCreateBusConnector(Switch);
  Result.OutletBus = OutletBus;
  if (!OutletBus) {
    FStructuralPowerTrace::LogPlacementSkip(Switch, TEXT("switch_bus_create_failed"),
                                            ELogVerbosity::Warning);
    Result.bDone = true;
    return Result;
  }

  const bool bInertPlace = !bWired && !Params.bKeyedSubnet;
  const bool bBypassOnStructure = bInertPlace && Params.bSwitchOn;

  FStructuralSiteMembershipParams MeshParams;
  MeshParams.bStripSwitchVanillaPortLinks = Params.bAdvancedWork || bFactRefresh;
  MeshParams.bBridgePeersOnly = true;
  MeshParams.bLinkVisibleConnections = bWired || Params.bKeyedSubnet || bBypassOnStructure;
  MeshParams.bMeshOnlyLinks = bInertPlace;
  MeshParams.bSkipEndpointIndexDirty = Root != INDEX_NONE;

  if (Site.bAnchored && Root != INDEX_NONE &&
      (bFactRefresh || !Session.Circuit().HasBridgeBusPeerMesh(OutletBus))) {
    IntegrateOnPlace(Session, Switch, OutletBus, SwitchId, EStructuralEndpointKind::Switch, Tracked,
                     Site, MeshParams);
  } else if (Site.bAnchored && Root != INDEX_NONE) {
    Tracked.Actor = Switch;
    Tracked.MountParentId = Site.MountParentId;
    Tracked.Kind = EStructuralEndpointKind::Switch;
    Session.RegisterBuildableActor(Switch);
    if (MeshParams.bLinkVisibleConnections) {
      Session.Circuit().LinkBusToVisibleConnectionsLocal(Switch, OutletBus,
                                                         MeshParams.bMeshOnlyLinks);
    }
  } else {
    Tracked.Actor = Switch;
    Tracked.MountParentId = Site.MountParentId;
    Tracked.Kind = EStructuralEndpointKind::Switch;
    Tracked.bAwaitingStructuralSite = true;
    Session.RegisterBuildableActor(Switch);

    if (Params.AttachContext != EAttachContext::WireDelta &&
        Params.AttachContext != EAttachContext::Toggle &&
        !Session.IsBuildablePlacementPending(Switch)) {
      FStructuralPowerTrace::LogPlacementSkip(Switch, TEXT("switch_site_not_ready"));
      Session.EnqueuePlacement(Switch, EStructuralPlacementJobType::Outlet, /*bDefer=*/true);
    }
    Result.bDone = true;
    return Result;
  }

  if (Site.bAnchored && Root != INDEX_NONE && Site.MountParentId.IsValid()) {
    Session.BridgeRootIndex().AddEndpointToRootIndex(Root, EStructuralEndpointKind::Switch,
                                                     SwitchId);
  } else {
    Session.BridgeRootIndex().MarkBridgeEndpointRootIndexDirty();
  }

  const bool bMeshed = Session.Circuit().HasBridgeBusPeerMesh(OutletBus);
  Result.bApplyBridgeStrategy = Params.AttachContext != EAttachContext::Toggle &&
                                !(Params.AttachContext == EAttachContext::WireDelta && bMeshed);
  return Result;
}

int32 FStructuralSiteMembership::SiteRootFromMount(FStructuralGraphSession& Session,
                                                   const FStructuralNodeId& MountParentId) {
  if (!MountParentId.IsValid()) {
    return INDEX_NONE;
  }

  return Session.StructureGraph().FindRoot(MountParentId);
}

bool FStructuralSiteMembership::ReaffirmMountParent(FStructuralGraphSession& Session,
                                                    AFGBuildable* Host, FTrackedEndpoint& Tracked,
                                                    const bool bUsePoleRootResolver) {
  if (!IsValid(Host)) {
    return false;
  }

  if (Tracked.MountParentId.IsValid()) {
    const int32 Root = SiteRootFromMount(Session, Tracked.MountParentId);
    if (Root != INDEX_NONE) {
      Tracked.bAwaitingStructuralSite = false;
      return true;
    }
  }

  FStructuralSiteContext Site;
  if (!ResolveSiteContext(Session, Host, Site, bUsePoleRootResolver) ||
      !Site.MountParentId.IsValid() || Site.SiteRoot == INDEX_NONE) {
    Tracked.bAwaitingStructuralSite = true;
    return false;
  }

  Tracked.MountParentId = Site.MountParentId;
  Tracked.bAwaitingStructuralSite = false;
  return true;
}
