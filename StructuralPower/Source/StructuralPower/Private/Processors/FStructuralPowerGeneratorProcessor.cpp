// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Processors/FStructuralPowerGeneratorProcessor.h"

#include "Attach/FStructuralBridgeAttach.h"
#include "Attach/FStructuralDeviceAttach.h"
#include "Attach/FStructuralPowerTransferGate.h"
#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableGenerator.h"
#include "Circuit/FStructuralGraphCircuitOps.h"
#include "Components/UFGStructuralPowerConnectionComponent.h"
#include "Config/FStructuralPowerModConfig.h"
#include "Core/FStructuralGraphSession.h"
#include "Core/FStructuralPowerContext.h"
#include "Diagnostics/FStructuralPowerTrace.h"
#include "FGCircuitConnectionComponent.h"
#include "FGPowerConnectionComponent.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Graph/FStructuralSiteContext.h"
#include "Network/UStructuralPowerMachineWireListener.h"
#include "Processors/FStructuralPowerMachineConsumerProcessor.h"
#include "Routing/FStructuralPowerRouter.h"
#include "StructuralPowerLog.h"

namespace {
void TearDownGeneratorBus(FStructuralGraphSession& Session, AFGBuildable* Host) {
  if (!IsValid(Host)) {
    return;
  }

  if (UFGPowerConnectionComponent* Plug =
          FStructuralDeviceAttach::FindFactoryWireConnection(Host)) {
    FStructuralDeviceAttach::TearDownConsumerLinks(Plug);
  }

  UFGStructuralPowerConnectionComponent* Bus = Session.FindBusConnector(Host);
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

void ProcessGenerationHost(FStructuralPowerContext& Ctx, AFGBuildable* Host) {
  if (!IsValid(Host)) {
    return;
  }

  FStructuralGraphSession& Session = Ctx.Session();
  const bool bBulk = Session.IsBulkLoadDrainActive();

  UFGPowerConnectionComponent* Plug = FStructuralDeviceAttach::FindFactoryWireConnection(Host);
  if (!IsValid(Plug)) {
    FStructuralPowerTrace::LogPlacementSkip(Host, TEXT("gen_plug_missing"));
    return;
  }

  if (FStructuralPowerTransferGate::IsConsumerVanillaWired(Plug)) {
    TearDownGeneratorBus(Session, Host);
    const FStructuralNodeId HostId = Session.MakeNodeId(Host);
    FTrackedEndpoint& Tracked = Session.TrackedEndpoints().FindOrAdd(HostId);
    Tracked.Actor = Host;
    Tracked.Kind = EStructuralEndpointKind::Generator;
    Tracked.bStructuralPowerTransferActive = false;
    Tracked.bAwaitingStructuralSite = false;
    Session.RegisterBuildableActor(Host);
    UStructuralPowerMachineWireListener::Ensure(Session.Owner(), Host);
    return;
  }

  UStructuralPowerMachineWireListener::Ensure(Session.Owner(), Host);

  if (!FStructuralPowerModConfig::IsGroupGenerationEnabled() ||
      !FStructuralPowerRouter::IsStructuralGeneratorRoutingEnabled()) {
    TearDownGeneratorBus(Session, Host);
    const FStructuralNodeId HostId = Session.MakeNodeId(Host);
    FTrackedEndpoint& Tracked = Session.TrackedEndpoints().FindOrAdd(HostId);
    Tracked.Actor = Host;
    Tracked.Kind = EStructuralEndpointKind::Generator;
    Tracked.bStructuralPowerTransferActive = false;
    Tracked.bAwaitingStructuralSite = false;
    Session.RegisterBuildableActor(Host);
    return;
  }

  // Own OutletBus + peer mesh — solo foundation pad has no pole host yet.
  FStructuralBridgeAttachRequest Request;
  Request.Host = Host;
  Request.Kind = EStructuralEndpointKind::Generator;
  Request.bUsePoleRootResolver = false;

  const FStructuralBridgeAttachOutcome Outcome =
      FStructuralBridgeAttach::AttachOnPlace(Ctx, Request);
  if (!Outcome.OutletBus && !(bBulk && Outcome.bAttached)) {
    if (!bBulk) {
      FStructuralPowerTrace::LogPlacementSkip(Host, TEXT("gen_bus_create_failed"),
                                              ELogVerbosity::Log);
    }
    return;
  }

  const FStructuralSiteContext& Site = Outcome.Site;
  if (!bBulk && !(Site.SiteRoot != INDEX_NONE && Site.MountParentId.IsValid())) {
    Session.BridgeRootIndex().MarkBridgeEndpointRootIndexDirty();
  }

  UE_LOG(LogStructuralPower, Log,
         TEXT("[HALSP] gen Process %s root=%d parent=%d attached=%d bus=%d"), *Host->GetName(),
         Site.SiteRoot, Site.bAnchored ? 1 : 0, Outcome.bAttached ? 1 : 0,
         Outcome.OutletBus ? 1 : 0);

  if (Outcome.bAttached && Site.SiteRoot != INDEX_NONE) {
    if (Outcome.OutletBus) {
      Session.Circuit().PromoteOutletBusIfPowered(Outcome.OutletBus, /*bLocalPromoteOnly=*/true);
    }
    FStructuralPowerMachineConsumerProcessor::EnqueueInactiveConsumersOnRoot(Session,
                                                                             Site.SiteRoot);
  } else if (!Outcome.bAttached && Outcome.bSiteNotReady) {
    FStructuralPowerTrace::LogPlacementSkip(Host, TEXT("gen_no_component"), ELogVerbosity::Log);
  } else if (!Outcome.bAttached) {
    FStructuralPowerTrace::LogPlacementSkip(Host, TEXT("gen_attach_failed"), ELogVerbosity::Log);
  }
}
}  // namespace

void FStructuralPowerGeneratorProcessor::Process(FStructuralPowerContext& Ctx,
                                                 AFGBuildableGenerator* Generator) {
  ProcessGenerationHost(Ctx, Generator);
}

void FStructuralPowerGeneratorProcessor::ProcessFactoryHost(FStructuralPowerContext& Ctx,
                                                            AFGBuildable* Host) {
  ProcessGenerationHost(Ctx, Host);
}

void FStructuralPowerGeneratorProcessor::TearDown(FStructuralPowerContext& Ctx,
                                                  AFGBuildableGenerator* Generator) {
  TearDownGeneratorBus(Ctx.Session(), Generator);
}

void FStructuralPowerGeneratorProcessor::TearDownFactoryHost(FStructuralPowerContext& Ctx,
                                                             AFGBuildable* Host) {
  TearDownGeneratorBus(Ctx.Session(), Host);
}
