// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Attach/FStructuralPanelAttach.h"

#include "Attach/FStructuralDeviceAttach.h"
#include "Buildables/FGBuildableLightSource.h"
#include "Buildables/FGBuildableLightsControlPanel.h"
#include "Circuit/FStructuralCircuitPromotionUtil.h"
#include "Circuit/FStructuralGraphCircuitOps.h"
#include "Components/UFGStructuralPowerConnectionComponent.h"
#include "Config/FStructuralPowerModConfig.h"
#include "Core/EStructuralPowerRole.h"
#include "Core/EStructuralPowerScope.h"
#include "Core/FStructuralGraphSession.h"
#include "Diagnostics/FStructuralPowerTrace.h"
#include "FGPowerConnectionComponent.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Panel/FStructuralPanelControlledSync.h"
#include "Panel/FStructuralPanelPortResolver.h"
#include "Routing/EStructuralChannel.h"
#include "Routing/FStructuralPowerRouter.h"
#include "Save/AStructuralPowerGraphSubsystem.h"
#include "StructuralPowerConstants.h"
#include "StructuralPowerLog.h"

void FStructuralPanelAttach::TearDownLinks(AFGBuildableControlPanelHost* Panel,
                                           const FStructuralPanelPorts& Ports) {
  auto Strip = [](UFGCircuitConnectionComponent* Conn) {
    if (UFGPowerConnectionComponent* Power = Cast<UFGPowerConnectionComponent>(Conn)) {
      FStructuralDeviceAttach::TearDownConsumerLinks(Power);
    }
  };

  Strip(Ports.Input);
  Strip(Ports.Downstream);

  if (UFGStructuralPowerConnectionComponent* ControlBus =
          AStructuralPowerGraphSubsystem::FindPanelControlBus(Panel)) {
    FStructuralDeviceAttach::TearDownConsumerLinks(ControlBus);
  }
}

void FStructuralPanelAttach::TearDownDownstreamLinks(AFGBuildableControlPanelHost* Panel,
                                                     const FStructuralPanelPorts& Ports) {
  if (UFGPowerConnectionComponent* Downstream =
          FStructuralPanelPortResolver::AsPowerConnection(Ports.Downstream)) {
    FStructuralDeviceAttach::TearDownConsumerLinks(Downstream);
  }

  if (UFGStructuralPowerConnectionComponent* ControlBus =
          AStructuralPowerGraphSubsystem::FindPanelControlBus(Panel)) {
    FStructuralDeviceAttach::TearDownConsumerLinks(ControlBus);
  }
}

static UFGPowerConnectionComponent* ResolvePanelFeedConnector(
    FStructuralGraphSession& Session, AFGBuildable* Panel, int32 ComponentRoot,
    const FStructuralChannelKey& PanelKey) {
  return Session.Circuit().ResolveSubnetFeedConnector(ComponentRoot, PanelKey);
}

bool FStructuralPanelAttach::SupplyAlreadyLinked(FStructuralGraphSession& Session,
                                                 AFGBuildableLightsControlPanel* Panel,
                                                 const FStructuralPanelPorts& Ports,
                                                 int32 ComponentRoot,
                                                 const FStructuralChannelKey& PanelKey) {
  UFGPowerConnectionComponent* InputPower =
      FStructuralPanelPortResolver::AsPowerConnection(Ports.Input);
  if (!IsValid(Panel) || !IsValid(InputPower) || ComponentRoot == INDEX_NONE ||
      PanelKey.Source.IsNone()) {
    return false;
  }

  UFGPowerConnectionComponent* Feed =
      ResolvePanelFeedConnector(Session, Panel, ComponentRoot, PanelKey);
  return Session.Circuit().IsPanelSupplyLinked(InputPower, Feed);
}

bool FStructuralPanelAttach::TryLinkSupply(FStructuralGraphSession& Session,
                                           AFGBuildableLightsControlPanel* Panel,
                                           const FStructuralPanelPorts& Ports, int32 ComponentRoot,
                                           const FStructuralChannelKey& PanelKey,
                                           bool bMeshOnlyLinks) {
  UFGPowerConnectionComponent* InputPower =
      FStructuralPanelPortResolver::AsPowerConnection(Ports.Input);
  if (!IsValid(Panel) || !IsValid(InputPower) || ComponentRoot == INDEX_NONE ||
      PanelKey.Source.IsNone()) {
    return false;
  }

  const FStructuralComponentKey CompKey = Session.Ids().MakeComponentKeyForRoot(ComponentRoot);
  if (!CompKey.IsValid()) {
    return false;
  }

  UFGPowerConnectionComponent* Feed =
      ResolvePanelFeedConnector(Session, Panel, ComponentRoot, PanelKey);
  if (!IsValid(Feed)) {
    return false;
  }

  if (InputPower->HasHiddenConnection(Feed)) {
    if (FStructuralPowerTrace::IsEnabled()) {
      FStructuralPowerTrace::LogConnector(TEXT("panel_supply"), Panel, InputPower);
      FStructuralPowerTrace::LogConnector(TEXT("panel_feed"), Panel, Feed);
    }
    return true;
  }

  const bool bLinked =
      Session.Circuit().LinkHiddenPair(InputPower, Feed, /*bPromoteCircuit=*/!bMeshOnlyLinks);
  if (FStructuralPowerTrace::IsEnabled()) {
    UE_LOG(LogStructuralPower, Log,
           TEXT("[HALSP] panel supply link %s -> %s ok=%d root=%d source=%s"), *Panel->GetName(),
           *Feed->GetName(), bLinked ? 1 : 0, ComponentRoot, *PanelKey.Source.ToString());
    FStructuralPowerTrace::LogConnector(TEXT("panel_supply"), Panel, InputPower);
    FStructuralPowerTrace::LogConnector(TEXT("panel_feed"), Panel, Feed);
  }
  return bLinked;
}

bool FStructuralPanelAttach::TryLinkLightToControlBus(FStructuralGraphSession& Session,
                                                      AFGBuildableLightsControlPanel* Panel,
                                                      AFGBuildableLightSource* Light) {
  if (!IsValid(Panel) || !IsValid(Light)) {
    return false;
  }

  const FName EffectiveControl =
      FStructuralPanelControlledSync::ResolveEffectiveLightControl(Session, Panel);
  if (EffectiveControl.IsNone()) {
    return false;
  }

  const FName LightSource = Session.Ids().ResolveSource(Light, EStructuralChannel::Light);
  if (LightSource != EffectiveControl) {
    return false;
  }

  UFGStructuralPowerConnectionComponent* ControlBus =
      AStructuralPowerGraphSubsystem::FindPanelControlBus(Panel);
  if (!IsValid(ControlBus)) {
    ControlBus = Session.GetOrCreatePanelControlBus(Panel);
  }
  if (!IsValid(ControlBus)) {
    return false;
  }

  UFGPowerConnectionComponent* Plug = FStructuralDeviceAttach::FindLightWireConnection(Light);
  if (!IsValid(Plug)) {
    return false;
  }

  if (ControlBus->HasHiddenConnection(Plug)) {
    return true;
  }

  if (!Session.Circuit().LinkHiddenPair(ControlBus, Plug)) {
    return false;
  }

  if (FStructuralPowerTrace::IsEnabled()) {
    const int32 Root = Session.BridgeRootIndex().GetEndpointComponentRoot(Panel);
    UE_LOG(LogStructuralPower, Log,
           TEXT("[HALSP] panel %s linked light %s kind=%s scope=%s site=%d role=%s"
                " path=panel_downstream control=%s"),
           *Panel->GetName(), *Light->GetName(),
           StructuralEndpointKindToString(EStructuralEndpointKind::Light),
           StructuralPowerScopeToString(EStructuralPowerScope::Site), Root,
           StructuralPowerRoleToString(EStructuralPowerRole::Host),
           *EffectiveControl.ToString());
    FStructuralPowerTrace::LogConnector(TEXT("panel_control_bus"), Panel, ControlBus);
    FStructuralPowerTrace::LogConnector(TEXT("light_plug"), Light, Plug);
  }

  return true;
}

bool FStructuralPanelAttach::AreKeyedLightsLinkedToControlBus(
    FStructuralGraphSession& Session, AFGBuildableLightsControlPanel* Panel,
    int32 ComponentRoot) {
  if (!IsValid(Panel) || ComponentRoot == INDEX_NONE) {
    return false;
  }

  const FName EffectiveControl =
      FStructuralPanelControlledSync::ResolveEffectiveLightControl(Session, Panel);
  if (EffectiveControl.IsNone()) {
    return false;
  }

  UFGStructuralPowerConnectionComponent* ControlBus =
      AStructuralPowerGraphSubsystem::FindPanelControlBus(Panel);
  if (!IsValid(ControlBus)) {
    return false;
  }

  FStructuralPanelPorts Ports;
  if (!FStructuralPanelPortResolver::Resolve(Panel, Ports)) {
    return false;
  }

  UFGPowerConnectionComponent* Downstream =
      FStructuralPanelPortResolver::AsPowerConnection(Ports.Downstream);
  if (!IsValid(Downstream) || !ControlBus->HasHiddenConnection(Downstream)) {
    return false;
  }

  bool bSawKeyedLight = false;
  bool bAllLinked = true;
  Session.Reconcile().EnumerateTrackedLightsOnRoot(
      ComponentRoot, [&](AFGBuildableLightSource* Light) {
        if (!IsValid(Light) || !bAllLinked) {
          return;
        }

        const FName LightSource = Session.Ids().ResolveSource(Light, EStructuralChannel::Light);
        if (LightSource != EffectiveControl) {
          return;
        }

        bSawKeyedLight = true;
        UFGPowerConnectionComponent* Plug =
            FStructuralDeviceAttach::FindLightWireConnection(Light);
        if (!IsValid(Plug) || !ControlBus->HasHiddenConnection(Plug)) {
          bAllLinked = false;
        }
      });

  return !bSawKeyedLight || bAllLinked;
}

AFGBuildableLightsControlPanel* FStructuralPanelAttach::FindPanelForDownstreamLight(
    FStructuralGraphSession& Session, int32 ComponentRoot,
    const FStructuralChannelKey& LightKey) {
  if (ComponentRoot == INDEX_NONE || LightKey.Source.IsNone()) {
    return nullptr;
  }

  auto PanelHostsLight = [&](AFGBuildableLightsControlPanel* Panel) -> bool {
    if (!IsValid(Panel)) {
      return false;
    }

    if (Session.BridgeRootIndex().GetEndpointComponentRoot(Panel) != ComponentRoot) {
      return false;
    }

    const FName PanelControl = Session.Ids().ResolveControl(Panel, EStructuralChannel::Light);
    return !PanelControl.IsNone() &&
           PanelControl != StructuralPowerConstants::ControlUnconfigured &&
           PanelControl == LightKey.Source;
  };

  Session.BridgeRootIndex().RefreshBridgeEndpointRootIndex();

  if (const TArray<FStructuralNodeId>* PanelIds =
          Session.EndpointIndex().Get(ComponentRoot, EStructuralEndpointKind::Panel)) {
    for (const FStructuralNodeId& PanelId : *PanelIds) {
      if (const FTrackedEndpoint* Tracked = Session.TrackedEndpoints().Find(PanelId)) {
        if (AFGBuildableLightsControlPanel* Panel = Tracked->GetPanel()) {
          if (PanelHostsLight(Panel)) {
            return Panel;
          }
        }
      }
    }
  }

  for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : Session.TrackedEndpoints()) {
    if (Pair.Value.Kind != EStructuralEndpointKind::Panel) {
      continue;
    }

    if (AFGBuildableLightsControlPanel* Panel = Pair.Value.GetPanel()) {
      if (PanelHostsLight(Panel)) {
        return Panel;
      }
    }
  }

  return nullptr;
}

void FStructuralPanelAttach::RestitchDownstream(FStructuralGraphSession& Session,
                                                AFGBuildableLightsControlPanel* Panel,
                                                const FStructuralPanelPorts& Ports,
                                                int32 ComponentRoot, FName PanelControl) {
  UFGPowerConnectionComponent* Downstream =
      FStructuralPanelPortResolver::AsPowerConnection(Ports.Downstream);
  if (!IsValid(Panel) || !IsValid(Downstream) || ComponentRoot == INDEX_NONE) {
    return;
  }

  const FName EffectiveControl =
      FStructuralPanelControlledSync::ResolveEffectiveLightControl(Session, Panel);
  if (EffectiveControl.IsNone()) {
    return;
  }

  UFGStructuralPowerConnectionComponent* ControlBus = Session.GetOrCreatePanelControlBus(Panel);
  if (!IsValid(ControlBus)) {
    return;
  }

  FStructuralDeviceAttach::TearDownConsumerLinks(ControlBus);
  FStructuralDeviceAttach::TearDownConsumerLinks(Downstream);

  if (!Session.Circuit().LinkHiddenPair(ControlBus, Downstream)) {
    return;
  }

  Session.Reconcile().EnumerateTrackedLightsOnRoot(
      ComponentRoot, [&](AFGBuildableLightSource* Light) {
        TryLinkLightToControlBus(Session, Panel, Light);
      });

  (void)PanelControl;
  (void)EffectiveControl;
  FStructuralPanelControlledSync::ApplyKeyedSubnet(Session, Panel);
}

void FStructuralPanelAttach::PromotePanelDownstreamSubnet(FStructuralGraphSession& Session,
                                                          AFGBuildableLightsControlPanel* Panel,
                                                          const FStructuralPanelPorts& Ports,
                                                          UFGPowerConnectionComponent* InputPower) {
  if (!IsValid(Panel) || !IsValid(InputPower) ||
      !FStructuralCircuitPromotionUtil::ConnectorSuppliesPower(InputPower)) {
    return;
  }

  Session.Circuit().PromoteStructuralMeshFrom(InputPower);

  UFGStructuralPowerConnectionComponent* ControlBus = Session.GetOrCreatePanelControlBus(Panel);
  if (IsValid(ControlBus) && FStructuralCircuitPromotionUtil::ComponentOnCircuit(ControlBus)) {
    Session.Circuit().PromoteStructuralMeshFrom(ControlBus);
  }
  (void)Ports;
}
