// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Attach/FStructuralSwitchWireEcho.h"

#include "Attach/FStructuralPowerTransferGate.h"
#include "Attach/FStructuralSwitchBridgeStrategy.h"
#include "Attach/FStructuralSwitchPredicates.h"
#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Core/FStructuralPowerContext.h"
#include "Diagnostics/FStructuralPowerTrace.h"
#include "FGPowerConnectionComponent.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Save/AStructuralPowerGraphSubsystem.h"

uint8 FStructuralSwitchWireEcho::BuildWireSignature(AFGBuildableCircuitSwitch* Switch) {
  if (!IsValid(Switch)) {
    return 0;
  }

  uint8 Signature = 0;
  if (const UFGPowerConnectionComponent* Conn0 =
          Cast<UFGPowerConnectionComponent>(Switch->GetConnection0())) {
    if (Conn0->GetNumConnections() > 0) {
      Signature |= 0x1;
    }
  }

  if (const UFGPowerConnectionComponent* Conn1 =
          Cast<UFGPowerConnectionComponent>(Switch->GetConnection1())) {
    if (Conn1->GetNumConnections() > 0) {
      Signature |= 0x2;
    }
  }

  return Signature;
}

void FStructuralSwitchWireEcho::OnCircuitsRebuilt(FStructuralPowerContext& Ctx,
                                                  AFGBuildableCircuitSwitch* Switch) {
  if (!IsValid(Switch)) {
    return;
  }

  const FStructuralNodeId SwitchId = Ctx.Session().MakeNodeId(Switch);
  FTrackedEndpoint& Tracked = Ctx.Session().TrackedEndpoints().FindOrAdd(SwitchId);
  const uint8 WireSignature = BuildWireSignature(Switch);
  if (Tracked.CachedSwitchWireSignature == WireSignature) {
    return;
  }

  if (FStructuralPowerTrace::IsExtendedDebugEnabled()) {
    FStructuralPowerTrace::LogHook(Switch, TEXT("OnCircuitsRebuilt"), TEXT("bridge_strategy"),
                                   TEXT("switch_circuits_rebuilt"));
  }

  const int32 Root = Tracked.MountParentId.IsValid()
                         ? Ctx.Session().StructureGraph().FindRoot(Tracked.MountParentId)
                         : INDEX_NONE;

  FStructuralSwitchBridgeStrategy::ApplyWireEcho(Ctx, Switch);

  if (FStructuralSwitchPredicates::NeedsAdvancedWork(Ctx, Switch) && Root != INDEX_NONE) {
    const bool bSwitchOn = Switch->IsBridgeActive();
    FStructuralPowerTransferGate::FlipBridgeGate(Ctx, Switch, bSwitchOn);
    if (!FStructuralSwitchPredicates::HasAssignedControl(Ctx, Switch)) {
      FStructuralPowerTransferGate::RefreshSiteStructuralConsumersOnRoot(Ctx, Root, bSwitchOn);
    }
  }

  Tracked.CachedSwitchWireSignature = WireSignature;
}
