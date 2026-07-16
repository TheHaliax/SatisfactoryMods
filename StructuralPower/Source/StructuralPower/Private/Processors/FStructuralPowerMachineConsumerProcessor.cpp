// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Processors/FStructuralPowerMachineConsumerProcessor.h"

#include "Attach/FStructuralDeviceAttach.h"
#include "Attach/FStructuralPowerTransferGate.h"
#include "Buildables/FGBuildable.h"
#include "Circuit/FStructuralGraphCircuitOps.h"
#include "Config/FStructuralPowerModConfig.h"
#include "Core/EAttachContext.h"
#include "Core/FStructuralGraphSession.h"
#include "Core/FStructuralPowerContext.h"
#include "Diagnostics/FStructuralPowerTrace.h"
#include "FGPowerConnectionComponent.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Graph/FStructuralPipeTopology.h"
#include "Lightweight/FStructuralLightweightTypes.h"
#include "Network/UStructuralPowerMachineWireListener.h"
#include "Save/AStructuralPowerGraphSubsystem.h"
#include "Save/FStructuralPlacementQueue.h"
#include "StructuralPowerLog.h"

namespace {
bool IsMachineConsumerKind(const EStructuralEndpointKind Kind) {
  return Kind == EStructuralEndpointKind::Extractor ||
         Kind == EStructuralEndpointKind::Manufacturer ||
         Kind == EStructuralEndpointKind::Transport || Kind == EStructuralEndpointKind::PipePump;
}
}  // namespace

void FStructuralPowerMachineConsumerProcessor::EnqueueInactiveConsumersOnRoot(
    FStructuralGraphSession& Session, const int32 Root) {
  if (Root == INDEX_NONE) {
    return;
  }

  for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : Session.TrackedEndpoints()) {
    if (!IsMachineConsumerKind(Pair.Value.Kind) || Pair.Value.bStructuralPowerTransferActive) {
      continue;
    }

    AFGBuildable* Device = Pair.Value.Actor.Get();
    if (!IsValid(Device)) {
      continue;
    }

    const int32 DeviceRoot = Pair.Value.MountParentId.IsValid()
                                 ? Session.StructureGraph().FindRoot(Pair.Value.MountParentId)
                                 : INDEX_NONE;
    if (DeviceRoot != Root) {
      continue;
    }

    Session.EnqueuePlacement(Device, EStructuralPlacementJobType::Outlet,
                             /*bDefer=*/true);
  }
}

void FStructuralPowerMachineConsumerProcessor::TearDown(FStructuralPowerContext& /*Ctx*/,
                                                        AFGBuildable* Device) {
  if (!IsValid(Device)) {
    return;
  }

  if (UFGPowerConnectionComponent* Plug =
          FStructuralDeviceAttach::FindFactoryWireConnection(Device)) {
    FStructuralDeviceAttach::TearDownConsumerLinks(Plug);
  }
}

void FStructuralPowerMachineConsumerProcessor::Process(FStructuralPowerContext& Ctx,
                                                       AFGBuildable* Device,
                                                       const EStructuralEndpointKind Kind,
                                                       bool (*IsGroupEnabled)()) {
  if (!IsValid(Device) || !IsGroupEnabled) {
    return;
  }

  // Parent/root before ChannelKey — ResolveChannelKey needs MountParentId for Source.
  const EAttachContext AttachContext = Ctx.GetAttachContext();
  const FStructuralWallAnchor ParentAnchor =
      Ctx.Session().BridgeRootIndex().ResolveOutletAnchor(Device);
  FStructuralNodeId ParentId;
  int32 Root =
      Ctx.Session().BridgeRootIndex().ResolveEndpointComponentRoot(Device, ParentAnchor, ParentId);

  // Pipe pumps: prefer structure site injected via pipe topology (support / machine).
  if (Kind == EStructuralEndpointKind::PipePump &&
      FStructuralPowerModConfig::IsGroupPipesEnabled()) {
    const int32 Injected =
        Ctx.Session().PipeTopology().ResolveInjectedStructureRoot(Device, Ctx.Session());
    if (Injected != INDEX_NONE) {
      Root = Injected;
      ParentId = Ctx.Session().StructureGraph().MakeCanonicalNodeIdForComponent(Injected);
    }
  }

  const FStructuralNodeId DeviceId = Ctx.Session().MakeNodeId(Device);
  FTrackedEndpoint& Tracked = Ctx.Session().TrackedEndpoints().FindOrAdd(DeviceId);
  Tracked.Actor = Device;
  Tracked.MountParentId = ParentId;
  Tracked.Kind = Kind;
  Ctx.Session().RegisterBuildableActor(Device);
  if (Root != INDEX_NONE) {
    if (IsBulkLoadAttachContext(AttachContext)) {
      Ctx.Session().BridgeRootIndex().AddEndpointToRootIndex(Root, Kind, DeviceId);
    } else {
      Ctx.Session().BridgeRootIndex().MarkBridgeEndpointRootIndexDirty();
    }
  }

  const FStructuralChannelKey ChannelKey =
      Ctx.Session().Ids().ResolveChannelKeyForBuildable(Device);

  UE_LOG(LogStructuralPower, Log,
         TEXT("[HALSP] machine Process %s kind=%d root=%d parent=%d group=%d"), *Device->GetName(),
         static_cast<int32>(Kind), Root, ParentAnchor.IsValid() ? 1 : 0, IsGroupEnabled() ? 1 : 0);

  UFGPowerConnectionComponent* Plug = FStructuralDeviceAttach::FindFactoryWireConnection(Device);
  if (!IsValid(Plug)) {
    FStructuralPowerTrace::LogPlacementSkip(Device, TEXT("machine_plug_missing"),
                                            ELogVerbosity::Log);
    return;
  }

  if (FStructuralPowerTransferGate::IsConsumerVanillaWired(Plug)) {
    FStructuralDeviceAttach::TearDownConsumerLinks(Plug);
    Tracked.bStructuralPowerTransferActive = false;
    Tracked.bAwaitingStructuralSite = false;
    UStructuralPowerMachineWireListener::Ensure(Ctx.Session().Owner(), Device);
    return;
  }

  UStructuralPowerMachineWireListener::Ensure(Ctx.Session().Owner(), Device);

  if (!IsGroupEnabled()) {
    FStructuralDeviceAttach::TearDownConsumerLinks(Plug);
    Tracked.bStructuralPowerTransferActive = false;
    Tracked.bAwaitingStructuralSite = false;
    return;
  }

  if (Root == INDEX_NONE) {
    Tracked.bStructuralPowerTransferActive = false;
    Tracked.bAwaitingStructuralSite = true;
    FStructuralPowerTrace::LogPlacementSkip(Device, TEXT("machine_no_component"),
                                            ELogVerbosity::Log);
    return;
  }

  const bool bWireOrToggle =
      FStructuralPowerTransferGate::IsBridgeWireOrToggleContext(AttachContext);
  if (!bWireOrToggle) {
    FStructuralDeviceAttach::TearDownConsumerLinks(Plug);
  }

  // Attach even if the bus isn't carrying power yet — GetComponentSourceConnector falls
  // back to any bridge bus. Power arrives when gens/poles energize the same root.
  const bool bAttached =
      FStructuralDeviceAttach::TryAttachConsumer(Ctx.Session(), Device, Plug, Root, ChannelKey);
  if (bAttached) {
    Tracked.bStructuralPowerTransferActive = true;
    Tracked.bAwaitingStructuralSite = false;
    Ctx.Session().Circuit().PromoteDirectHiddenLinks(Plug);

    if (Kind != EStructuralEndpointKind::PipePump &&
        FStructuralPowerModConfig::IsGroupPipesEnabled()) {
      Ctx.Session().PipeTopology().RegisterMachineInject(Device, Root, Ctx.Session());
    }
    return;
  }

  const bool bFirstFail = !Tracked.bAwaitingStructuralSite;
  Tracked.bStructuralPowerTransferActive = false;
  Tracked.bAwaitingStructuralSite = true;
  const bool bFeed = Ctx.Session().Circuit().DoesComponentRootCarryPower(Root) ||
                     Ctx.Session().Circuit().DoesSiteStructuralBusCarryPower(Root);
  FStructuralPowerTrace::LogPlacementSkip(
      Device, bFeed ? TEXT("machine_attach_failed") : TEXT("machine_no_bridge_bus"),
      ELogVerbosity::Log);

  // One deferred retry after first fail (index/feed race); no enqueue loop.
  if (bFirstFail && !Ctx.Session().IsBuildablePlacementPending(Device)) {
    Ctx.Session().EnqueuePlacement(Device, EStructuralPlacementJobType::Outlet,
                                   /*bDefer=*/true);
  }
}
