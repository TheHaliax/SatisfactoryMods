// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Processors/FStructuralEndpointDispatcher.h"

#include "Attach/FStructuralBridgeAttach.h"
#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Buildables/FGBuildableLightsControlPanel.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "Config/FStructuralPowerModConfig.h"
#include "Core/EAttachContext.h"
#include "Core/FStructuralGraphSession.h"
#include "Core/FStructuralPowerContext.h"
#include "Diagnostics/FStructuralPowerTrace.h"
#include "Diagnostics/FStructuralPowerTraceScope.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Graph/FStructuralGraphCircuitEcho.h"
#include "Graph/FStructuralPoleWireUtil.h"
#include "Panel/FStructuralPanelPortResolver.h"
#include "Processors/FStructuralEndpointCatalog.h"
#include "Processors/IStructuralEndpointProcessor.h"

namespace {
IStructuralEndpointProcessor* FindMutableProcessor(const AFGBuildable* Buildable) {
  const IStructuralEndpointProcessor* Processor =
      FStructuralEndpointCatalog::Get().Classify(Buildable);
  if (!Processor) {
    return nullptr;
  }

  return FStructuralEndpointCatalog::Get().FindMutable(Processor->GetBuildableKind());
}

FStructuralPowerContext MakePlacementContext(FStructuralGraphSession& Session,
                                             const bool bOverrideAttachContext,
                                             const EAttachContext AttachContextOverride,
                                             const int32 SiteRoot = INDEX_NONE) {
  if (bOverrideAttachContext) {
    return Session.MakeProcessorContext(AttachContextOverride, SiteRoot);
  }

  return Session.GetProcessorContext();
}
}  // namespace

void FStructuralEndpointDispatcher::RunPlacement(FStructuralGraphSession& Session,
                                                 IStructuralEndpointProcessor& Processor,
                                                 AFGBuildable* Host, const bool bLocalPromoteOnly,
                                                 const bool bOverrideAttachContext,
                                                 const EAttachContext AttachContextOverride) {
  if (!IsValid(Host)) {
    return;
  }

  const FStructuralEndpointDescriptor& Descriptor = Processor.GetDescriptor();
  if (Descriptor.GroupGate && !Descriptor.GroupGate()) {
    return;
  }

  FStructuralPowerContext Ctx =
      MakePlacementContext(Session, bOverrideAttachContext, AttachContextOverride);
  Processor.ProcessPlacement(Ctx, Host, bLocalPromoteOnly);
}

void FStructuralEndpointDispatcher::RunWireDelta(FStructuralGraphSession& Session,
                                                 IStructuralEndpointProcessor& Processor,
                                                 AFGBuildable* Host,
                                                 const EAttachContext AttachContext) {
  if (!IsValid(Host)) {
    return;
  }

  FStructuralPowerContext Ctx = Session.MakeProcessorContext(AttachContext);
  Processor.OnWireDelta(Ctx, Host);
}

void FStructuralEndpointDispatcher::RunCircuitsRebuilt(FStructuralGraphSession& Session,
                                                       IStructuralEndpointProcessor& Processor,
                                                       AFGBuildable* Host,
                                                       const bool bOverrideAttachContext,
                                                       const EAttachContext AttachContextOverride,
                                                       const int32 SiteRoot) {
  if (!IsValid(Host)) {
    return;
  }

  FStructuralPowerContext Ctx =
      MakePlacementContext(Session, bOverrideAttachContext, AttachContextOverride, SiteRoot);
  Processor.OnCircuitsRebuilt(Ctx, Host);
}

void FStructuralEndpointDispatcher::RunToggleChanged(FStructuralGraphSession& Session,
                                                     IStructuralEndpointProcessor& Processor,
                                                     AFGBuildable* Host) {
  if (!IsValid(Host)) {
    return;
  }

  FStructuralPowerContext Ctx = Session.MakeProcessorContext(EAttachContext::Toggle);
  Processor.OnToggleChanged(Ctx, Host);
}

void FStructuralEndpointDispatcher::RunTeardown(FStructuralGraphSession& Session,
                                                IStructuralEndpointProcessor& Processor,
                                                AFGBuildable* Host) {
  if (!IsValid(Host)) {
    return;
  }

  FStructuralPowerContext Ctx = Session.GetProcessorContext();
  Processor.TearDown(Ctx, Host);
}

bool FStructuralEndpointDispatcher::ShouldSkipOutletDuringBulkLoad(
    const FStructuralGraphSession& Session, const AFGBuildable* Buildable) {
  if (!Session.IsBulkLoadDrainActive()) {
    return false;
  }

  if (const IStructuralEndpointProcessor* Processor =
          FStructuralEndpointCatalog::Get().Classify(Buildable)) {
    const FStructuralEndpointDescriptor& Descriptor = Processor->GetDescriptor();
    // Lights/panels/machines wait for remesh. Mid-bulk gen attach fails
    // (no source connector yet) — post-load machine workers retry (lights pattern).
    return Descriptor.Kind == EStructuralEndpointKind::Light ||
           Descriptor.Kind == EStructuralEndpointKind::Panel ||
           Descriptor.Kind == EStructuralEndpointKind::Generator ||
           Descriptor.Kind == EStructuralEndpointKind::Extractor ||
           Descriptor.Kind == EStructuralEndpointKind::Manufacturer ||
           Descriptor.Kind == EStructuralEndpointKind::Transport ||
           Descriptor.Kind == EStructuralEndpointKind::PipePump;
  }

  return false;
}

void FStructuralEndpointDispatcher::DispatchPlacement(FStructuralGraphSession& Session,
                                                      AFGBuildable* Buildable,
                                                      const bool bLocalPromoteOnly,
                                                      const bool bOverrideAttachContext,
                                                      const EAttachContext AttachContextOverride) {
  if (IStructuralEndpointProcessor* Processor = FindMutableProcessor(Buildable)) {
    RunPlacement(Session, *Processor, Buildable, bLocalPromoteOnly, bOverrideAttachContext,
                 AttachContextOverride);
  }
}

void FStructuralEndpointDispatcher::DispatchOutlet(FStructuralGraphSession& Session,
                                                   AFGBuildable* Buildable) {
  if (!IsValid(Buildable)) {
    return;
  }

  if (ShouldSkipOutletDuringBulkLoad(Session, Buildable)) {
    return;
  }

  if (!FindMutableProcessor(Buildable)) {
    FStructuralPowerTrace::LogPlacementSkip(Buildable, TEXT("not_bridge_endpoint"));
    return;
  }

  DispatchPlacement(Session, Buildable, /*bLocalPromoteOnly=*/false);
}

void FStructuralEndpointDispatcher::DispatchTeardown(FStructuralGraphSession& Session,
                                                     AFGBuildable* Buildable) {
  if (IStructuralEndpointProcessor* Processor = FindMutableProcessor(Buildable)) {
    RunTeardown(Session, *Processor, Buildable);
  }
}

void FStructuralEndpointDispatcher::DispatchSwitchCircuitsRebuilt(
    FStructuralGraphSession& Session, AFGBuildableCircuitSwitch* Switch) {
  HALSP_TRACE_SCOPE_DETAIL(TEXT("mod"), TEXT("circuitsRebuilt.switch"),
                           IsValid(Switch) ? Switch->GetName() : TEXT("null"));

  if (!IsValid(Switch) || !Switch->HasAuthority()) {
    return;
  }

  if (Session.ShouldDeferCircuitDrivenRefresh()) {
    return;
  }

  if (Session.CircuitEcho().ShouldSkipSwitchCircuitEcho(Switch)) {
    return;
  }

  const FStructuralNodeId SwitchId = Session.MakeNodeId(Switch);
  const FTrackedEndpoint* Tracked = Session.TrackedEndpoints().Find(SwitchId);
  if (!Tracked || !Tracked->MountParentId.IsValid()) {
    return;
  }

  const int32 Root = Session.StructureGraph().FindRoot(Tracked->MountParentId);
  if (IStructuralEndpointProcessor* Processor =
          FStructuralEndpointCatalog::Get().FindMutable(EStructuralEndpointKind::Switch)) {
    RunCircuitsRebuilt(Session, *Processor, Switch,
                       /*bOverrideAttachContext=*/true, EAttachContext::WireDelta, Root);
  }

  Session.CircuitEcho().NoteSwitchCircuitEchoProcessed(Switch);
}

void FStructuralEndpointDispatcher::DispatchPanelWireDelta(FStructuralGraphSession& Session,
                                                           AFGBuildableLightsControlPanel* Panel) {
  HALSP_TRACE_SCOPE_DETAIL(TEXT("mod"), TEXT("wireDelta.panel"),
                           IsValid(Panel) ? Panel->GetName() : TEXT("null"));

  if (!IsValid(Panel) || !Panel->HasAuthority()) {
    return;
  }

  if (!FStructuralPowerModConfig::IsGroupLightingEnabled() ||
      Session.ShouldDeferCircuitDrivenRefresh()) {
    return;
  }

  if (Session.CircuitEcho().ShouldSkipPanelCircuitEcho(Panel)) {
    return;
  }

  FStructuralPanelPorts Ports;
  const bool bPortsResolved = FStructuralPanelPortResolver::Resolve(Panel, Ports);
  const FStructuralNodeId PanelId = Session.MakeNodeId(Panel);
  const FTrackedEndpoint* Existing = Session.TrackedEndpoints().Find(PanelId);
  if (!bPortsResolved && (!Existing || Existing->Kind != EStructuralEndpointKind::Panel ||
                          !Existing->MountParentId.IsValid())) {
    return;
  }

  FStructuralPowerTrace::LogHook(Panel, TEXT("OnCircuitsRebuilt"), TEXT("wire_refresh"),
                                 TEXT("panel_wire_delta"));

  if (Existing && Existing->Kind == EStructuralEndpointKind::Panel &&
      Existing->MountParentId.IsValid()) {
    DispatchPlacement(Session, Panel,
                      /*bLocalPromoteOnly=*/true,
                      /*bOverrideAttachContext=*/true, EAttachContext::WireDelta);
    return;
  }

  DispatchPlacement(Session, Panel, /*bLocalPromoteOnly=*/true);
}

void FStructuralEndpointDispatcher::DispatchPoleWireDelta(FStructuralGraphSession& Session,
                                                          AFGBuildablePowerPole* Pole) {
  if (!IsValid(Pole) || !Pole->HasAuthority()) {
    return;
  }

  if (Session.ShouldDeferCircuitDrivenRefresh()) {
    Session.EnqueuePlacement(Pole, EStructuralPlacementJobType::Outlet, /*bDefer=*/true);
    return;
  }

  if (!FStructuralBridgeAttach::HasPlacementMembership(Session, Pole,
                                                       EStructuralEndpointKind::Pole)) {
    const FStructuralNodeId PoleId = Session.MakeNodeId(Pole);
    const FTrackedEndpoint* Tracked = Session.TrackedEndpoints().Find(PoleId);
    if (Tracked && Tracked->Kind == EStructuralEndpointKind::Pole &&
        !FStructuralPoleWireUtil::HasVanillaWire(Pole)) {
      return;
    }

    DispatchPlacement(Session, Pole);
    return;
  }

  DispatchPlacement(Session, Pole,
                    /*bLocalPromoteOnly=*/false,
                    /*bOverrideAttachContext=*/true, EAttachContext::WireDelta);
}

void FStructuralEndpointDispatcher::DispatchWallOutletAfterWire(FStructuralGraphSession& Session,
                                                                AFGBuildablePowerPole* Pole) {
  if (!IsValid(Pole) || !Pole->HasAuthority()) {
    return;
  }

  FStructuralPowerTrace::LogHook(Pole, TEXT("OnPowerConnectionChanged"), TEXT("wire_refresh"),
                                 TEXT("pole_wire_delta"));
  DispatchPoleWireDelta(Session, Pole);
}
