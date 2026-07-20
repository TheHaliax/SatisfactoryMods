// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Processors/FStructuralPowerSwitchProcessor.h"

#include "Attach/FStructuralPowerTransferGate.h"
#include "Attach/FStructuralSwitchBridgeStrategy.h"
#include "Attach/FStructuralSwitchPredicates.h"
#include "Attach/FStructuralSwitchWireEcho.h"
#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Circuit/FStructuralCircuitPromotionScope.h"
#include "Circuit/FStructuralCircuitPromotionUtil.h"
#include "Components/UFGStructuralPowerConnectionComponent.h"
#include "Connection/FStructuralSiteMembership.h"
#include "Core/EAttachContext.h"
#include "Core/EStructuralPowerRole.h"
#include "Core/EStructuralPowerScope.h"
#include "Core/FStructuralGraphSession.h"
#include "Core/FStructuralPowerContext.h"
#include "Diagnostics/FStructuralPowerTrace.h"
#include "Diagnostics/FStructuralPowerTraceScope.h"
#include "FGCircuitConnectionComponent.h"
#include "Graph/FStructuralOutletParentResolver.h"
#include "Graph/FStructuralSwitchParentResolver.h"
#include "Network/UStructuralPowerSwitchListener.h"
#include "Routing/EStructuralChannel.h"
#include "Save/AStructuralPowerGraphSubsystem.h"
#include "Save/FStructuralPlacementQueue.h"
#include "StructuralPowerLog.h"

void FStructuralPowerSwitchProcessor::TearDown(FStructuralPowerContext& Ctx,
                                               AFGBuildableCircuitSwitch* Switch) {
  if (!IsValid(Switch)) {
    return;
  }

  UFGStructuralPowerConnectionComponent* Bus = Ctx.Session().FindBusConnector(Switch);
  if (!IsValid(Bus)) {
    return;
  }

  TArray<UFGCircuitConnectionComponent*> HiddenLinks;
  Bus->GetHiddenConnections(HiddenLinks);
  for (UFGCircuitConnectionComponent* OtherRaw : HiddenLinks) {
    Bus->RemoveHiddenConnection(OtherRaw);
    if (IsValid(OtherRaw)) {
      OtherRaw->RemoveHiddenConnection(Bus);
    }
  }
}

void FStructuralPowerSwitchProcessor::OnCircuitsRebuilt(FStructuralPowerContext& Ctx,
                                                        AFGBuildableCircuitSwitch* Switch) {
  FStructuralSwitchWireEcho::OnCircuitsRebuilt(Ctx, Switch);
}

int32 FStructuralPowerSwitchProcessor::ResolveMountRoot(FStructuralPowerContext& Ctx,
                                                        const FStructuralWallAnchor& Anchor,
                                                        FStructuralNodeId& OutParentId) {
  OutParentId = FStructuralNodeId();
  if (!Anchor.IsValid()) {
    return INDEX_NONE;
  }

  OutParentId = Ctx.Session().BridgeRootIndex().MakeParentNodeId(Anchor);
  const int32 Root = Ctx.Session().StructureGraph().FindRoot(OutParentId);
  if (Root != INDEX_NONE) {
    return Root;
  }

  if (IsBulkLoadAttachContext(Ctx.GetAttachContext())) {
    return INDEX_NONE;
  }

  if (Ctx.Session().BridgeRootIndex().EnsureParentRegisteredInGraph(Anchor, OutParentId)) {
    return Ctx.Session().StructureGraph().FindRoot(OutParentId);
  }

  return INDEX_NONE;
}

int32 FStructuralPowerSwitchProcessor::ResolveToggleSiteRoot(FStructuralPowerContext& Ctx,
                                                             AFGBuildableCircuitSwitch* Switch,
                                                             FTrackedEndpoint& Tracked) {
  FStructuralNodeId ParentId = Tracked.MountParentId;
  int32 Root = Ctx.Session().BridgeRootIndex().FindRootForTrackedEndpoint(Tracked);

  const FStructuralOutletParentResolveParams ParentParams =
      Ctx.Session().BridgeRootIndex().MakeOutletParentResolveParams();
  auto TryResolveFromParent = [&](bool bPreferWirePort) -> int32 {
    const FStructuralSwitchParentResolveResult ParentResolve =
        FStructuralSwitchParentResolver::Resolve(
            Switch, Ctx.Session().GetWorld(), Ctx.Session().StructureGraph(),
            Ctx.Session().LightweightIndex(), bPreferWirePort, &ParentParams);
    FStructuralNodeId ResolvedParentId;
    const int32 ResolvedRoot = ResolveMountRoot(Ctx, ParentResolve.Anchor, ResolvedParentId);
    if (ResolvedRoot != INDEX_NONE) {
      ParentId = ResolvedParentId;
    }
    return ResolvedRoot;
  };

  if (Root == INDEX_NONE) {
    Root = TryResolveFromParent(/*bPreferWirePort=*/false);
  }

  if (Root == INDEX_NONE && FStructuralSwitchParentResolver::IsWiredToStructureSide(Switch)) {
    Root = TryResolveFromParent(/*bPreferWirePort=*/true);
  }

  if (Root == INDEX_NONE && FStructuralSwitchParentResolver::IsWiredToStructureSide(Switch)) {
    FStructuralSwitchParentResolver::ForEachWiredStructureSideAnchor(
        Switch, Ctx.Session().GetWorld(), Ctx.Session().LightweightIndex(), &ParentParams,
        [&](const FStructuralWallAnchor& Anchor) {
          if (Root != INDEX_NONE) {
            return;
          }

          FStructuralNodeId ResolvedParentId;
          const int32 ResolvedRoot = ResolveMountRoot(Ctx, Anchor, ResolvedParentId);
          if (ResolvedRoot != INDEX_NONE) {
            Root = ResolvedRoot;
            ParentId = ResolvedParentId;
          }
        });
  }

  if (Root != INDEX_NONE) {
    Tracked.MountParentId = ParentId;
    Ctx.Session().BridgeRootIndex().MarkBridgeEndpointRootIndexDirty();
  }

  return Root;
}

void FStructuralPowerSwitchProcessor::EnsureListener(FStructuralPowerContext& Ctx,
                                                     AFGBuildableCircuitSwitch* Switch) {
  if (!IsValid(Switch)) {
    return;
  }

  FStructuralGraphSession& Session = Ctx.Session();
  UStructuralPowerSwitchListener* Listener = nullptr;

  TInlineComponentArray<UStructuralPowerSwitchListener*> Listeners;
  Switch->GetComponents(Listeners);
  if (Listeners.Num() > 0) {
    Listener = Listeners[0];
  } else {
    Listener = NewObject<UStructuralPowerSwitchListener>(Switch, NAME_None, RF_Transient);
    if (!Listener) {
      return;
    }

    Switch->AddInstanceComponent(Listener);
    Listener->RegisterComponent();
    Listener->BindSubsystem(&Session.Owner(), Switch);
    return;
  }

  Listener->SyncSubscriptions(&Session.Owner(), Switch);
}

void FStructuralPowerSwitchProcessor::OnStateChanged(FStructuralPowerContext& Ctx,
                                                     AFGBuildableCircuitSwitch* Switch) {
  if (!IsValid(Switch) || !Switch->HasAuthority()) {
    return;
  }

  if (IsBulkLoadAttachContext(Ctx.GetAttachContext())) {
    return;
  }

  const FStructuralNodeId SwitchId = Ctx.Session().MakeNodeId(Switch);
  if (FTrackedEndpoint* Tracked = Ctx.Session().TrackedEndpoints().Find(SwitchId)) {
    FStructuralCircuitPromotionScope PromotionScope(&Ctx.Session().Owner());

    int32 Root = ResolveToggleSiteRoot(Ctx, Switch, *Tracked);

    const FStructuralChannelKey SwitchKey =
        Ctx.Session().Ids().ResolveChannelKeyForBuildable(Switch);
    const bool bKeyedSubnet = FStructuralSwitchPredicates::HasAssignedControl(Ctx, Switch);
    const bool bSwitchOn = Switch->IsBridgeActive();

    Ctx.Session().CircuitEcho().MarkEchoDirtyForSwitchToggle(Switch, Root);
    Ctx.Session().CircuitEcho().NoteSwitchToggleHandled(Switch);

    FStructuralSwitchBridgeStrategy::ApplyToggle(Ctx, Switch);

    FStructuralPowerTrace::LogHook(Switch, TEXT("OnIsSwitchOnChanged"),
                                   bSwitchOn ? TEXT("transfer_on") : TEXT("transfer_off"), nullptr);

    FStructuralPowerTransferGate::ApplyToggleTransferSideEffects(Ctx, Switch, Root, bKeyedSubnet,
                                                                 SwitchKey.Control, bSwitchOn);

    return;
  }

  Ctx.Session().EnqueuePlacement(Switch, EStructuralPlacementJobType::Outlet, /*bDefer=*/true);
}

void FStructuralPowerSwitchProcessor::ApplyStructureMembership(FStructuralPowerContext& Ctx,
                                                               AFGBuildableCircuitSwitch* Switch) {
  if (!IsValid(Switch)) {
    return;
  }

  FStructuralGraphSession& Session = Ctx.Session();
  const bool bSwitchOn = Switch->IsBridgeActive();
  const bool bKeyedSubnet = FStructuralSwitchPredicates::HasAssignedControl(Ctx, Switch);
  const bool bAdvancedWork = FStructuralSwitchPredicates::NeedsAdvancedWork(Ctx, Switch);

  FStructuralSwitchMembershipParams MembershipParams;
  MembershipParams.bAdvancedWork = bAdvancedWork;
  MembershipParams.bKeyedSubnet = bKeyedSubnet;
  MembershipParams.bSwitchOn = bSwitchOn;
  MembershipParams.AttachContext = Ctx.GetAttachContext();

  const FStructuralSwitchMembershipResult Membership =
      FStructuralSiteMembership::IntegrateSwitchOnPlace(Session, Switch, MembershipParams);

  if (Membership.bDone) {
    const FStructuralNodeId SwitchId = Session.MakeNodeId(Switch);
    if (FTrackedEndpoint* Tracked = Session.TrackedEndpoints().Find(SwitchId)) {
      Tracked->CachedSwitchWireSignature = FStructuralSwitchPredicates::BuildWireSignature(Switch);
    }
    return;
  }

  if (Membership.bApplyBridgeStrategy) {
    FStructuralSwitchBridgeStrategy::ApplyMembership(Ctx, Switch);
  }

  const FStructuralNodeId SwitchId = Session.MakeNodeId(Switch);
  if (FTrackedEndpoint* Tracked = Session.TrackedEndpoints().Find(SwitchId)) {
    Tracked->CachedSwitchWireSignature = FStructuralSwitchPredicates::BuildWireSignature(Switch);
  }
}

void FStructuralPowerSwitchProcessor::Process(FStructuralPowerContext& Ctx,
                                              AFGBuildableCircuitSwitch* Switch) {
  HALSP_TRACE_SCOPE_DETAIL(TEXT("mod"), TEXT("switch.Process"),
                           IsValid(Switch) ? Switch->GetName() : TEXT("null"));

  if (!IsValid(Switch)) {
    FStructuralPowerTrace::LogPlacementSkip(Switch, TEXT("switch_gating_disabled"));
    return;
  }

  FStructuralGraphSession& Session = Ctx.Session();
  const bool bBulk = Session.IsBulkLoadDrainActive();
  const EAttachContext AttachContext = Ctx.GetAttachContext();
  const FStructuralNodeId SwitchId = Session.MakeNodeId(Switch);

  const bool bAdvancedWork = FStructuralSwitchPredicates::NeedsAdvancedWork(Ctx, Switch);
  if (bAdvancedWork) {
    FStructuralPowerTransferGate::FlipBridgeGate(Ctx, Switch, Switch->IsBridgeActive());
  }

  ApplyStructureMembership(Ctx, Switch);

  UFGStructuralPowerConnectionComponent* OutletBus = Session.FindBusConnector(Switch);
  if (!OutletBus) {
    return;
  }

  FTrackedEndpoint* Tracked = Session.TrackedEndpoints().Find(SwitchId);
  if (!Tracked) {
    return;
  }

  int32 Root = INDEX_NONE;
  if (Tracked->MountParentId.IsValid()) {
    Root = Session.StructureGraph().FindRoot(Tracked->MountParentId);
  }

  const FStructuralOutletParentResolveParams ParentParams =
      Session.BridgeRootIndex().MakeOutletParentResolveParams();
  const FStructuralSwitchParentResolveResult ParentResolve =
      FStructuralSwitchParentResolver::Resolve(Switch, Session.GetWorld(), Session.StructureGraph(),
                                               Session.LightweightIndex(),
                                               /*bPreferWirePort=*/false, &ParentParams);
  const TCHAR* WirePort = ParentResolve.WirePortIndex == 0
                              ? TEXT("A")
                              : (ParentResolve.WirePortIndex == 1 ? TEXT("B") : TEXT("-"));

  const bool bSwitchOn = Switch->IsBridgeActive();
  const bool bKeyedSubnet = FStructuralSwitchPredicates::HasAssignedControl(Ctx, Switch);
  const bool bWired = FStructuralSwitchParentResolver::HasAnyVanillaWire(Switch);

  if (bWired && Root != INDEX_NONE && !IsBulkLoadAttachContext(AttachContext)) {
    FStructuralPowerTransferGate::PropagateWiredFeedChange(Ctx, Switch, Root);
  }

  const FStructuralChannelKey ChannelKey = Session.Ids().ResolveChannelKeyForBuildable(Switch);

  const TCHAR* Mode = (!bWired && !bKeyedSubnet) ? TEXT("inert")
                                                 : (bSwitchOn ? AttachContextToString(AttachContext)
                                                              : TEXT("bridge_idle"));

  UE_LOG(
      LogStructuralPower, Log,
      TEXT("[HALSP] outlet %s kind=%s scope=%s site=%d role=%s root=%d parentValid=%d busCircuit=%d"
           " powered=%d tag=%s source=%s control=%s wirePort=%s mode=%s"),
      *Switch->GetName(), StructuralEndpointKindToString(EStructuralEndpointKind::Switch),
      StructuralPowerScopeToString(EStructuralPowerScope::Site), Root,
      StructuralPowerRoleToString(EStructuralPowerRole::Router), Root, Root != INDEX_NONE ? 1 : 0,
      OutletBus->GetCircuitID(),
      FStructuralCircuitPromotionUtil::ComponentCarriesPower(OutletBus) ? 1 : 0,
      StructuralChannelToString(ChannelKey.Tag),
      *FStructuralPowerTrace::FormatSourceForTrace(ChannelKey),
      *FStructuralPowerTrace::FormatControlForTrace(ChannelKey), WirePort, Mode);

  if (bBulk) {
    return;
  }

  if (bSwitchOn && Root != INDEX_NONE && bKeyedSubnet && !bWired) {
    FStructuralPowerTransferGate::ApplyKeyedTransferOnRoot(Ctx, Root, ChannelKey.Control,
                                                           /*bGateOpen=*/true,
                                                           /*bLocalPromoteOnly=*/true);
  }

  if (AttachContext == EAttachContext::WireDelta) {
    FStructuralPowerTransferGate::ApplyWireDeltaTransferSideEffects(Ctx, Switch, Root);
  }

  EnsureListener(Ctx, Switch);
}

void FStructuralPowerSwitchProcessor::OnWireDelta(FStructuralPowerContext& Ctx,
                                                  AFGBuildableCircuitSwitch* Switch) {
  if (!IsValid(Switch) || Ctx.GetAttachContext() == EAttachContext::Toggle) {
    return;
  }

  FStructuralPowerSwitchProcessor::Process(Ctx, Switch);
}
