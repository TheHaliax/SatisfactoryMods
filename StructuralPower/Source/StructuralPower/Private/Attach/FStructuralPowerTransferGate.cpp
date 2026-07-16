// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Attach/FStructuralPowerTransferGate.h"

#include "Attach/FStructuralDeviceAttach.h"
#include "Attach/FStructuralPanelAttach.h"
#include "Attach/FStructuralSwitchPredicates.h"
#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Buildables/FGBuildableLightSource.h"
#include "Buildables/FGBuildableLightsControlPanel.h"
#include "Config/FStructuralPowerModConfig.h"
#include "Core/EAttachContext.h"
#include "Core/EStructuralPowerRole.h"
#include "Core/EStructuralPowerScope.h"
#include "Core/FStructuralGraphSession.h"
#include "Core/FStructuralPowerContext.h"
#include "Diagnostics/FStructuralPowerTrace.h"
#include "FGPowerConnectionComponent.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Panel/FStructuralPanelControlledSync.h"
#include "Panel/FStructuralPanelPortResolver.h"
#include "Processors/FStructuralPowerBridgeProcessor.h"
#include "Processors/FStructuralPowerLightProcessor.h"
#include "Routing/EStructuralChannel.h"
#include "Routing/FStructuralPowerRouter.h"
#include "Save/AStructuralPowerGraphSubsystem.h"
#include "StructuralPowerConstants.h"
#include "StructuralPowerLog.h"

namespace {
void SetTrackedTransfer(FTrackedEndpoint& Tracked, bool bActive) {
  Tracked.bStructuralPowerTransferActive = bActive;
}

bool RootAllowsTransfer(FStructuralPowerContext& Ctx, int32 Root) {
  return Root != INDEX_NONE && (Ctx.Session().Circuit().DoesComponentRootCarryPower(Root) ||
                                Ctx.Session().Circuit().DoesSiteStructuralBusCarryPower(Root));
}

FName ResolveSwitchGateId(const FStructuralChannelKey& ChannelKey) {
  return ChannelKey.Control.IsNone() ? ChannelKey.Source : ChannelKey.Control;
}

void SuspendPanelTransfer(FStructuralPowerContext& Ctx, AFGBuildableLightsControlPanel* Panel,
                          FTrackedEndpoint& Tracked) {
  SetTrackedTransfer(Tracked, false);
  Tracked.bPanelLinksReady = false;
  Tracked.bDownstreamLinksReady = false;

  FStructuralPanelPorts Ports;
  if (FStructuralPanelPortResolver::Resolve(Panel, Ports)) {
    if (UFGPowerConnectionComponent* InputPower =
            FStructuralPanelPortResolver::AsPowerConnection(Ports.Input)) {
      FStructuralDeviceAttach::TearDownConsumerLinks(InputPower);
    }

    FStructuralPanelAttach::TearDownDownstreamLinks(Panel, Ports);
  }

  if (FStructuralPowerTrace::IsEnabled()) {
    UE_LOG(LogStructuralPower, Log, TEXT("[HALSP] panel transfer_off %s attach=%s"),
           *Panel->GetName(), AttachContextToString(Ctx.GetAttachContext()));
  }
}

void SuspendLightTransfer(FStructuralPowerContext& Ctx, AFGBuildableLightSource* Light,
                          FTrackedEndpoint& Tracked) {
  SetTrackedTransfer(Tracked, false);

  if (UFGPowerConnectionComponent* Plug = FStructuralDeviceAttach::FindLightWireConnection(Light)) {
    FStructuralDeviceAttach::TearDownConsumerLinks(Plug);
  }

  if (FStructuralPowerTrace::IsEnabled()) {
    UE_LOG(LogStructuralPower, Log, TEXT("[HALSP] light transfer_off %s attach=%s"),
           *Light->GetName(), AttachContextToString(Ctx.GetAttachContext()));
  }
}
}  // namespace

bool FStructuralPowerTransferGate::IsBridgeWireOrToggleContext(EAttachContext AttachContext) {
  return AttachContext == EAttachContext::WireDelta || AttachContext == EAttachContext::Toggle;
}

bool FStructuralPowerTransferGate::IsConsumerVanillaWired(
    const UFGPowerConnectionComponent* VisiblePlug) {
  return IsValid(VisiblePlug) && VisiblePlug->GetNumConnections() > 0;
}

void FStructuralPowerTransferGate::FlipBridgeGate(FStructuralPowerContext& Ctx,
                                                  AFGBuildableCircuitSwitch* Switch,
                                                  bool bGateOpen) {
  if (!IsValid(Switch)) {
    return;
  }

  const FStructuralNodeId SwitchId = Ctx.Session().MakeNodeId(Switch);
  FTrackedEndpoint& Tracked = Ctx.Session().TrackedEndpoints().FindOrAdd(SwitchId);
  SetTrackedTransfer(Tracked, bGateOpen);

  UE_LOG(
      LogStructuralPower, Log,
      TEXT("[HALSP] switch transfer_%s %s kind=%s scope=%s site=%d role=%s attach=%s transfer=%d"),
      bGateOpen ? TEXT("on") : TEXT("off"), *Switch->GetName(),
      StructuralEndpointKindToString(EStructuralEndpointKind::Switch),
      StructuralPowerScopeToString(EStructuralPowerScope::Site), Ctx.GetSiteRoot(),
      StructuralPowerRoleToString(EStructuralPowerRole::Gateway),
      AttachContextToString(Ctx.GetAttachContext()), bGateOpen ? 1 : 0);
}

void FStructuralPowerTransferGate::ApplyKeyedTransferOnRoot(FStructuralPowerContext& Ctx,
                                                            int32 Root, FName SwitchControlId,
                                                            bool bGateOpen,
                                                            bool bLocalPromoteOnly) {
  if (Root == INDEX_NONE || !FStructuralPowerRouter::IsAssignedControl(SwitchControlId)) {
    return;
  }

  const bool bAllowTransfer = bGateOpen && RootAllowsTransfer(Ctx, Root);

  Ctx.Session().BridgeRootIndex().RefreshBridgeEndpointRootIndex();

  auto ApplyPanel = [&](const FStructuralNodeId& NodeId) {
    FTrackedEndpoint* Tracked = Ctx.Session().TrackedEndpoints().Find(NodeId);
    if (!Tracked) {
      return;
    }

    AFGBuildableLightsControlPanel* Panel = Tracked->GetPanel();
    if (!IsValid(Panel)) {
      return;
    }

    if (Ctx.Session().Ids().ResolveSource(Panel, EStructuralChannel::Light) != SwitchControlId) {
      return;
    }

    const bool bPanelTransfer =
        bAllowTransfer && FStructuralPowerModConfig::IsGroupLightingEnabled();
    SetTrackedTransfer(*Tracked, bPanelTransfer);
    if (bPanelTransfer) {
      FStructuralPowerBridgeProcessor::ApplyLocalAttachForPanel(Ctx, Panel, bLocalPromoteOnly);
      FStructuralPanelControlledSync::ApplyKeyedSubnet(Ctx.Session(), Panel);
    } else if (!bGateOpen || !FStructuralPowerModConfig::IsGroupLightingEnabled()) {
      SuspendPanelTransfer(Ctx, Panel, *Tracked);
    }
  };

  auto ApplyLight = [&](const FStructuralNodeId& NodeId) {
    FTrackedEndpoint* Tracked = Ctx.Session().TrackedEndpoints().Find(NodeId);
    if (!Tracked) {
      return;
    }

    AFGBuildableLightSource* Light = Tracked->GetLight();
    if (!IsValid(Light)) {
      return;
    }

    const FStructuralChannelKey LightKey = Ctx.Session().Ids().ResolveChannelKeyForBuildable(Light);
    if (LightKey.Source != SwitchControlId ||
        Ctx.Session().Reconcile().IsPanelDownstreamLight(Root, LightKey)) {
      return;
    }

    const bool bVanillaWired =
        IsConsumerVanillaWired(FStructuralDeviceAttach::FindLightWireConnection(Light));
    const bool bLightTransfer =
        bAllowTransfer && FStructuralPowerModConfig::IsGroupLightingEnabled() && !bVanillaWired;
    SetTrackedTransfer(*Tracked, bLightTransfer);

    if (bLightTransfer) {
      FStructuralPowerLightProcessor::Process(Ctx, Light, bLocalPromoteOnly);
    } else if (!bGateOpen || !FStructuralPowerModConfig::IsGroupLightingEnabled()) {
      SuspendLightTransfer(Ctx, Light, *Tracked);
    }
  };

  if (const TArray<FStructuralNodeId>* PanelIds =
          Ctx.Session().EndpointIndex().Get(Root, EStructuralEndpointKind::Panel)) {
    const TArray<FStructuralNodeId> Snapshot = *PanelIds;
    for (const FStructuralNodeId& NodeId : Snapshot) {
      ApplyPanel(NodeId);
    }
  }

  if (const TArray<FStructuralNodeId>* LightIds =
          Ctx.Session().EndpointIndex().Get(Root, EStructuralEndpointKind::Light)) {
    const TArray<FStructuralNodeId> Snapshot = *LightIds;
    for (const FStructuralNodeId& NodeId : Snapshot) {
      ApplyLight(NodeId);
    }
  }
}

void FStructuralPowerTransferGate::SuspendAllKeyedLightingTransfer(FStructuralPowerContext& Ctx) {
  Ctx.Session().BridgeRootIndex().RefreshBridgeEndpointRootIndex();

  TSet<int32> Roots;
  for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : Ctx.Session().TrackedEndpoints()) {
    if (Pair.Value.Kind != EStructuralEndpointKind::Switch) {
      continue;
    }

    AFGBuildableCircuitSwitch* Switch = Pair.Value.GetSwitch();
    if (!IsValid(Switch)) {
      continue;
    }

    const FName Control = Ctx.Session().Ids().ResolveControl(Switch, EStructuralChannel::Switch);
    if (!FStructuralPowerRouter::IsAssignedControl(Control)) {
      continue;
    }

    const int32 Root = Ctx.Session().StructureGraph().FindRoot(Pair.Value.MountParentId);
    if (Root == INDEX_NONE) {
      continue;
    }

    Roots.Add(Root);
    ApplyKeyedTransferOnRoot(Ctx, Root, Control,
                             /*bGateOpen=*/false,
                             /*bLocalPromoteOnly=*/false);
  }

  if (FStructuralPowerTrace::IsEnabled()) {
    UE_LOG(LogStructuralPower, Log, TEXT("[HALSP] suspended keyed lighting transfer on %d root(s)"),
           Roots.Num());
  }
}

void FStructuralPowerTransferGate::RefreshKeyedTransferOnRoot(FStructuralPowerContext& Ctx,
                                                              int32 Root, bool bLocalPromoteOnly) {
  if (Root == INDEX_NONE) {
    return;
  }

  if (FStructuralPowerTrace::IsEnabled()) {
    UE_LOG(LogStructuralPower, Log, TEXT("[HALSP] keyed transfer refresh root=%d attach=%s"), Root,
           AttachContextToString(Ctx.GetAttachContext()));
  }

  if (Ctx.Session().BridgeEndpointRootIndexDirty()) {
    Ctx.Session().BridgeRootIndex().RefreshBridgeEndpointRootIndex();
  }

  const TArray<FStructuralNodeId>* SwitchIds =
      Ctx.Session().EndpointIndex().Get(Root, EStructuralEndpointKind::Switch);
  if (!SwitchIds || SwitchIds->Num() == 0) {
    return;
  }

  const TArray<FStructuralNodeId> SwitchSnapshot = *SwitchIds;
  for (const FStructuralNodeId& NodeId : SwitchSnapshot) {
    const FTrackedEndpoint* Tracked = Ctx.Session().TrackedEndpoints().Find(NodeId);
    if (!Tracked) {
      continue;
    }

    AFGBuildableCircuitSwitch* Switch = Tracked->GetSwitch();
    if (!IsValid(Switch) || !Switch->IsBridgeActive()) {
      continue;
    }

    const FName Control = Ctx.Session().Ids().ResolveControl(Switch, EStructuralChannel::Switch);
    if (!FStructuralPowerRouter::IsAssignedControl(Control)) {
      continue;
    }

    ApplyKeyedTransferOnRoot(Ctx, Root, Control,
                             /*bGateOpen=*/true, bLocalPromoteOnly);
  }

  RefreshSiteStructuralConsumersOnRoot(Ctx, Root, RootAllowsTransfer(Ctx, Root));
}

void FStructuralPowerTransferGate::RefreshSiteStructuralConsumersOnRoot(
    FStructuralPowerContext& Ctx, int32 Root, bool bGateOpen) {
  if (Root == INDEX_NONE || !FStructuralPowerModConfig::IsGroupLightingEnabled()) {
    return;
  }

  Ctx.Session().BridgeRootIndex().RefreshBridgeEndpointRootIndex();

  const FStructuralComponentKey CompKey = Ctx.Session().Ids().MakeComponentKeyForRoot(Root);
  const FName ComponentDefaultId =
      CompKey.IsValid() ? Ctx.Session().Ids().GetOrCreateComponentDefaultId(CompKey) : NAME_None;

  const bool bAllowTransfer = bGateOpen && RootAllowsTransfer(Ctx, Root);

  auto RefreshFoundationLight = [&](const FStructuralNodeId& NodeId) {
    FTrackedEndpoint* Tracked = Ctx.Session().TrackedEndpoints().Find(NodeId);
    if (!Tracked) {
      return;
    }

    AFGBuildableLightSource* Light = Tracked->GetLight();
    if (!IsValid(Light)) {
      return;
    }

    const FStructuralChannelKey LightKey = Ctx.Session().Ids().ResolveChannelKeyForBuildable(Light);
    if (LightKey.Source.IsNone() || LightKey.Source != ComponentDefaultId ||
        Ctx.Session().Reconcile().IsPanelDownstreamLight(Root, LightKey)) {
      return;
    }

    const bool bVanillaWired =
        IsConsumerVanillaWired(FStructuralDeviceAttach::FindLightWireConnection(Light));
    const bool bLightTransfer = bAllowTransfer && !bVanillaWired;
    SetTrackedTransfer(*Tracked, bLightTransfer);

    if (bLightTransfer) {
      FStructuralPowerLightProcessor::Process(Ctx, Light, /*bLocalPromoteOnly=*/true);
    } else {
      SuspendLightTransfer(Ctx, Light, *Tracked);
    }
  };

  if (const TArray<FStructuralNodeId>* LightIds =
          Ctx.Session().EndpointIndex().Get(Root, EStructuralEndpointKind::Light)) {
    const TArray<FStructuralNodeId> Snapshot = *LightIds;
    for (const FStructuralNodeId& NodeId : Snapshot) {
      RefreshFoundationLight(NodeId);
    }
  }
}

void FStructuralPowerTransferGate::PropagateWiredFeedChange(FStructuralPowerContext& Ctx,
                                                            AFGBuildableCircuitSwitch* Switch,
                                                            int32 LocalRoot) {
  FStructuralPowerBridgeProcessor::PropagateCrossSiteFeedChange(Ctx, Switch, LocalRoot);
}

void FStructuralPowerTransferGate::ApplyWireDeltaTransferSideEffects(
    FStructuralPowerContext& Ctx, AFGBuildableCircuitSwitch* Switch, int32 Root) {
  if (!IsValid(Switch) || Root == INDEX_NONE) {
    return;
  }

  const bool bAdvancedWork = FStructuralSwitchPredicates::NeedsAdvancedWork(Ctx, Switch);
  const bool bSwitchOn = Switch->IsBridgeActive();
  const bool bGateOpen = bAdvancedWork && bSwitchOn;

  if (bAdvancedWork) {
    FlipBridgeGate(Ctx, Switch, bSwitchOn);

    if (FStructuralSwitchPredicates::HasAssignedControl(Ctx, Switch)) {
      const FStructuralChannelKey SwitchKey =
          Ctx.Session().Ids().ResolveChannelKeyForBuildable(Switch);
      ApplyKeyedTransferOnRoot(Ctx, Root, SwitchKey.Control, bSwitchOn,
                               /*bLocalPromoteOnly=*/true);
    }
  } else {
    FlipBridgeGate(Ctx, Switch, false);
  }

  if (!FStructuralSwitchPredicates::HasAssignedControl(Ctx, Switch)) {
    RefreshSiteStructuralConsumersOnRoot(Ctx, Root, bGateOpen);
  }
}

void FStructuralPowerTransferGate::ApplyToggleTransferSideEffects(FStructuralPowerContext& Ctx,
                                                                  AFGBuildableCircuitSwitch* Switch,
                                                                  int32 Root, bool bKeyedSubnet,
                                                                  FName SwitchControlId,
                                                                  bool bSwitchOn) {
  if (!IsValid(Switch) || !FStructuralSwitchPredicates::NeedsAdvancedWork(Ctx, Switch)) {
    return;
  }

  FlipBridgeGate(Ctx, Switch, bSwitchOn);

  if (Root != INDEX_NONE && bKeyedSubnet) {
    ApplyKeyedTransferOnRoot(Ctx, Root, SwitchControlId, bSwitchOn,
                             /*bLocalPromoteOnly=*/true);
  }

  if (Root != INDEX_NONE && !bKeyedSubnet) {
    RefreshSiteStructuralConsumersOnRoot(Ctx, Root, bSwitchOn);
  }
}
