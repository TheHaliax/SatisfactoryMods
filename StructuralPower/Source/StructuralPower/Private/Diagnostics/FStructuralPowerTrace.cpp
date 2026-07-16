// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Diagnostics/FStructuralPowerTrace.h"

#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableLightSource.h"
#include "Buildables/FGBuildableLightsControlPanel.h"
#include "Circuit/FStructuralCircuitPromotionUtil.h"
#include "Config/FStructuralPowerModConfig.h"
#include "Core/EStructuralPowerRole.h"
#include "Core/EStructuralPowerScope.h"
#include "FGCircuitConnectionComponent.h"
#include "FGPowerConnectionComponent.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Panel/FStructuralPanelPortResolver.h"
#include "Routing/EStructuralChannel.h"
#include "Routing/FStructuralPowerRouter.h"
#include "Save/AStructuralPowerGraphSubsystem.h"
#include "StructuralPowerConstants.h"
#include "StructuralPowerLog.h"

FStructuralConnectorPowerSnapshot FStructuralConnectorPowerSnapshot::From(
    const UFGCircuitConnectionComponent* Connector) {
  FStructuralConnectorPowerSnapshot Snap;
  if (!IsValid(Connector)) {
    return Snap;
  }

  const UFGPowerConnectionComponent* Power = Cast<UFGPowerConnectionComponent>(Connector);
  if (!IsValid(Power)) {
    Snap.Circuit = Connector->GetCircuitID();
    Snap.bConnected = Connector->IsConnected() || Connector->GetNumConnections() > 0;
    return Snap;
  }

  Snap.Circuit = Power->GetCircuitID();
  Snap.bConnected = Power->IsConnected() || Power->GetNumConnections() > 0;
  Snap.bCarriesPower = FStructuralCircuitPromotionUtil::ComponentCarriesPower(Power);
  Snap.bHasPower = Power->HasPower();
  Snap.bSuppliesPower = FStructuralCircuitPromotionUtil::ConnectorSuppliesPower(Power);
  return Snap;
}

FString FStructuralConnectorPowerSnapshot::Format() const {
  return FString::Printf(TEXT("circuit=%d conn=%d carries=%d hasPower=%d supplies=%d"), Circuit,
                         bConnected ? 1 : 0, bCarriesPower ? 1 : 0, bHasPower ? 1 : 0,
                         bSuppliesPower ? 1 : 0);
}

bool FStructuralPowerTrace::IsEnabled() {
  return FStructuralPowerModConfig::IsTraceEnabled();
}

bool FStructuralPowerTrace::IsExtendedDebugEnabled() {
  return FStructuralPowerModConfig::IsExtendedDebugEnabled();
}

FString FStructuralPowerTrace::FormatEffectiveIdForTrace(EStructuralChannel Tag,
                                                         FName EffectiveId) {
  if (Tag == EStructuralChannel::Structure) {
    return TEXT("-");
  }

  return EffectiveId.IsNone() ? TEXT("?") : EffectiveId.ToString();
}

FString FStructuralPowerTrace::FormatSourceForTrace(const FStructuralChannelKey& Key) {
  if (Key.Tag == EStructuralChannel::Structure) {
    return TEXT("-");
  }

  if (FStructuralPowerRouter::UsesSourceControlModel(Key.Tag)) {
    return Key.Source.IsNone() ? TEXT("?") : Key.Source.ToString();
  }

  return FormatEffectiveIdForTrace(Key.Tag, Key.EffectiveId);
}

FString FStructuralPowerTrace::FormatControlForTrace(const FStructuralChannelKey& Key) {
  if (!FStructuralPowerRouter::UsesSourceControlModel(Key.Tag)) {
    return TEXT("-");
  }

  if (Key.Control.IsNone()) {
    return TEXT("-");
  }

  if (Key.Control == StructuralPowerConstants::ControlUnconfigured) {
    return TEXT("UNCONFIGURED");
  }

  return Key.Control.ToString();
}

FStructuralChannelKey FStructuralPowerTrace::KeyForBuildable(AFGBuildable* Buildable) {
  if (!IsValid(Buildable)) {
    return {};
  }

  if (UWorld* World = Buildable->GetWorld()) {
    if (AStructuralPowerGraphSubsystem* Graph = AStructuralPowerGraphSubsystem::Find(World)) {
      return Graph->ResolveChannelKeyForBuildable(Buildable);
    }
  }

  return {};
}

void FStructuralPowerTrace::LogHook(AFGBuildable* Buildable, const TCHAR* Hook, const TCHAR* Action,
                                    const TCHAR* Detail) {
  if (!IsValid(Buildable)) {
    return;
  }

  if (!IsEnabled() || !IsExtendedDebugEnabled()) {
    return;
  }

  const FStructuralChannelKey Key = KeyForBuildable(Buildable);
  UE_LOG(LogStructuralPower, Log,
         TEXT("[HALSP] hook %s %s %s class=%s tag=%s source=%s control=%s detail=%s"),
         Hook ? Hook : TEXT("?"), *Buildable->GetName(), Action ? Action : TEXT("?"),
         *Buildable->GetClass()->GetName(), StructuralChannelToString(Key.Tag),
         *FormatSourceForTrace(Key), *FormatControlForTrace(Key), Detail ? Detail : TEXT(""));
}

void FStructuralPowerTrace::LogPlacementSkip(AFGBuildable* Buildable, const TCHAR* Reason,
                                             ELogVerbosity::Type Verbosity) {
  if (!IsValid(Buildable)) {
    return;
  }

  const TCHAR* ReasonText = Reason ? Reason : TEXT("?");

  switch (Verbosity) {
    case ELogVerbosity::Verbose:
      UE_LOG(LogStructuralPower, Verbose, TEXT("[HALSP] skip %s class=%s reason=%s"),
             *Buildable->GetName(), *Buildable->GetClass()->GetName(), ReasonText);
      break;
    case ELogVerbosity::Log:
      UE_LOG(LogStructuralPower, Log, TEXT("[HALSP] skip %s class=%s reason=%s"),
             *Buildable->GetName(), *Buildable->GetClass()->GetName(), ReasonText);
      break;
    default:
      UE_LOG(LogStructuralPower, Verbose, TEXT("[HALSP] skip %s class=%s reason=%s"),
             *Buildable->GetName(), *Buildable->GetClass()->GetName(), ReasonText);
      break;
  }
}

void FStructuralPowerTrace::LogLinkOp(const TCHAR* Op, UFGCircuitConnectionComponent* A,
                                      UFGCircuitConnectionComponent* B, bool bSuccess,
                                      const TCHAR* Path, ELogVerbosity::Type Verbosity) {
  if (!Op) {
    return;
  }

  // Hot path: never touch subsystem / KeyForBuildable or Log-level I/O here.
  // ExtendedDebug adds connector detail; otherwise skip (scopes cover timing).
  if (!IsExtendedDebugEnabled()) {
    return;
  }

  const int32 CircuitA = IsValid(A) ? A->GetCircuitID() : INDEX_NONE;
  const int32 CircuitB = IsValid(B) ? B->GetCircuitID() : INDEX_NONE;
  const bool bHadLink = IsValid(A) && IsValid(B) && A->HasHiddenConnection(B);
  const FStructuralConnectorPowerSnapshot SnapA = FStructuralConnectorPowerSnapshot::From(A);
  const FStructuralConnectorPowerSnapshot SnapB = FStructuralConnectorPowerSnapshot::From(B);
  const FStructuralChannelKey KeyA =
      KeyForBuildable(Cast<AFGBuildable>(IsValid(A) ? A->GetOwner() : nullptr));
  const FStructuralChannelKey KeyB =
      KeyForBuildable(Cast<AFGBuildable>(IsValid(B) ? B->GetOwner() : nullptr));

  if (Verbosity == ELogVerbosity::Verbose) {
    UE_LOG(LogStructuralPower, Verbose,
           TEXT("[HALSP] link %s ok=%d path=%s hadLink=%d A(circuit=%d tag=%s src=%s ctl=%s %s)"
                " B(circuit=%d tag=%s src=%s ctl=%s %s)"),
           Op, bSuccess ? 1 : 0, Path ? Path : TEXT("?"), bHadLink ? 1 : 0, CircuitA,
           StructuralChannelToString(KeyA.Tag), *FormatSourceForTrace(KeyA),
           *FormatControlForTrace(KeyA), *SnapA.Format(), CircuitB,
           StructuralChannelToString(KeyB.Tag), *FormatSourceForTrace(KeyB),
           *FormatControlForTrace(KeyB), *SnapB.Format());
  } else {
    UE_LOG(LogStructuralPower, Log,
           TEXT("[HALSP] link %s ok=%d path=%s hadLink=%d A(circuit=%d tag=%s src=%s ctl=%s %s)"
                " B(circuit=%d tag=%s src=%s ctl=%s %s)"),
           Op, bSuccess ? 1 : 0, Path ? Path : TEXT("?"), bHadLink ? 1 : 0, CircuitA,
           StructuralChannelToString(KeyA.Tag), *FormatSourceForTrace(KeyA),
           *FormatControlForTrace(KeyA), *SnapA.Format(), CircuitB,
           StructuralChannelToString(KeyB.Tag), *FormatSourceForTrace(KeyB),
           *FormatControlForTrace(KeyB), *SnapB.Format());
  }
}

void FStructuralPowerTrace::LogConnector(const TCHAR* Role, AFGBuildable* Owner,
                                         UFGCircuitConnectionComponent* Connector) {
  if (!IsEnabled() || !Role) {
    return;
  }

  const FStructuralConnectorPowerSnapshot Snap = FStructuralConnectorPowerSnapshot::From(Connector);
  UE_LOG(LogStructuralPower, Log, TEXT("[HALSP] connector %s owner=%s role=%s %s"), Role,
         IsValid(Owner) ? *Owner->GetName() : TEXT("null"),
         IsValid(Connector) ? *Connector->GetName() : TEXT("null"), *Snap.Format());
}

void FStructuralPowerTrace::LogLightConsumer(AFGBuildableLightSource* Light, int32 Root,
                                             bool bParentValid, const FStructuralChannelKey& Key,
                                             UFGPowerConnectionComponent* Plug, const TCHAR* Path,
                                             int32 PanelSupplyReady, int32 PanelDownstreamFed) {
  if (!IsValid(Light)) {
    return;
  }

  const FStructuralConnectorPowerSnapshot Snap = FStructuralConnectorPowerSnapshot::From(Plug);
  const bool bPanelDownstream = Path && FCString::Strcmp(Path, TEXT("panel_downstream")) == 0;
  if (bPanelDownstream) {
    const bool bPanelFed = PanelSupplyReady > 0;
    const bool bArmedOn = Light->ShouldLightBeOn();
    const bool bPass = bPanelFed && bArmedOn;
    if (PanelSupplyReady >= 0 || PanelDownstreamFed >= 0) {
      UE_LOG(LogStructuralPower, Log,
             TEXT("[HALSP] light %s kind=%s scope=%s site=%d role=%s root=%d parentValid=%d tag=%s"
                  " source=%s control=%s path=%s plugCircuit=%s armedOn=%d enabled=%d"
                  " panelFed=%d downstreamFed=%d pass=%d"),
             *Light->GetName(), StructuralEndpointKindToString(EStructuralEndpointKind::Light),
             StructuralPowerScopeToString(EStructuralPowerScope::Site), Root,
             StructuralPowerRoleToString(EStructuralPowerRole::Host), Root, bParentValid ? 1 : 0,
             StructuralChannelToString(Key.Tag), *FormatSourceForTrace(Key),
             *FormatControlForTrace(Key), Path, *Snap.Format(), bArmedOn ? 1 : 0,
             Light->IsLightEnabled() ? 1 : 0, PanelSupplyReady >= 0 ? PanelSupplyReady : -1,
             PanelDownstreamFed >= 0 ? PanelDownstreamFed : -1, bPass ? 1 : 0);
    } else {
      UE_LOG(LogStructuralPower, Log,
             TEXT("[HALSP] light %s kind=%s scope=%s site=%d role=%s root=%d parentValid=%d tag=%s"
                  " source=%s control=%s path=%s plugCircuit=%s armedOn=%d enabled=%d"),
             *Light->GetName(), StructuralEndpointKindToString(EStructuralEndpointKind::Light),
             StructuralPowerScopeToString(EStructuralPowerScope::Site), Root,
             StructuralPowerRoleToString(EStructuralPowerRole::Host), Root, bParentValid ? 1 : 0,
             StructuralChannelToString(Key.Tag), *FormatSourceForTrace(Key),
             *FormatControlForTrace(Key), Path, *Snap.Format(), bArmedOn ? 1 : 0,
             Light->IsLightEnabled() ? 1 : 0);
    }
    return;
  }

  const int32 PlugLit = IsValid(Plug) && Plug->HasPower() ? 1 : 0;
  UE_LOG(LogStructuralPower, Log,
         TEXT("[HALSP] light %s kind=%s scope=%s site=%d role=%s root=%d parentValid=%d tag=%s"
              " source=%s control=%s path=%s plugCircuit=%s lit=%d"),
         *Light->GetName(), StructuralEndpointKindToString(EStructuralEndpointKind::Light),
         StructuralPowerScopeToString(EStructuralPowerScope::Site), Root,
         StructuralPowerRoleToString(EStructuralPowerRole::Host), Root, bParentValid ? 1 : 0,
         StructuralChannelToString(Key.Tag), *FormatSourceForTrace(Key),
         *FormatControlForTrace(Key), Path ? Path : TEXT("-"), *Snap.Format(), PlugLit);
}

void FStructuralPowerTrace::LogPanelConsumer(AFGBuildableLightsControlPanel* Panel, int32 Root,
                                             const FStructuralChannelKey& Key,
                                             const FStructuralPanelPorts& Ports, bool bSupplyReady,
                                             int32 ControlledCount, const TCHAR* Context) {
  if (!IsValid(Panel)) {
    return;
  }

  const FStructuralConnectorPowerSnapshot Upstream = FStructuralConnectorPowerSnapshot::From(
      FStructuralPanelPortResolver::AsPowerConnection(Ports.Input));
  const FStructuralConnectorPowerSnapshot ControlBus = FStructuralConnectorPowerSnapshot::From(
      AStructuralPowerGraphSubsystem::FindPanelControlBus(Panel));
  const FStructuralConnectorPowerSnapshot Downstream = FStructuralConnectorPowerSnapshot::From(
      FStructuralPanelPortResolver::AsPowerConnection(Ports.Downstream));

  UE_LOG(
      LogStructuralPower, Log,
      TEXT("[HALSP] panel %s kind=%s ctx=%s scope=%s site=%d role=%s root=%d source=%s control=%s"
           " panelFed=%d controlled=%d upstream={%s} controlBus={%s} downstream={%s}"),
      *Panel->GetName(), StructuralEndpointKindToString(EStructuralEndpointKind::Panel),
      Context ? Context : TEXT("?"), StructuralPowerScopeToString(EStructuralPowerScope::Site),
      Root, StructuralPowerRoleToString(EStructuralPowerRole::Router), Root,
      *FormatSourceForTrace(Key), *FormatControlForTrace(Key), bSupplyReady ? 1 : 0,
      ControlledCount, *Upstream.Format(), *ControlBus.Format(), *Downstream.Format());
}
