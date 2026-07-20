// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Network/UStructuralPowerMachineWireListener.h"

#include "Buildables/FGBuildable.h"
#include "Core/EAttachContext.h"
#include "FGCircuitConnectionComponent.h"
#include "FGPowerConnectionComponent.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Processors/FStructuralEndpointCatalog.h"
#include "Processors/FStructuralEndpointDispatcher.h"
#include "Processors/IStructuralEndpointProcessor.h"
#include "Save/AStructuralPowerGraphSubsystem.h"

void UStructuralPowerMachineWireListener::Ensure(AStructuralPowerGraphSubsystem& Graph,
                                                 AFGBuildable* Host) {
  if (!IsValid(Host) || !Host->HasAuthority()) {
    return;
  }

  TInlineComponentArray<UStructuralPowerMachineWireListener*> Existing;
  Host->GetComponents(Existing);
  if (Existing.Num() > 0 && IsValid(Existing[0])) {
    Existing[0]->BindSubsystem(&Graph, Host);
    return;
  }

  UStructuralPowerMachineWireListener* Listener =
      NewObject<UStructuralPowerMachineWireListener>(Host, NAME_None, RF_Transient);
  if (!Listener) {
    return;
  }

  Host->AddInstanceComponent(Listener);
  Listener->RegisterComponent();
  Listener->BindSubsystem(&Graph, Host);
}

void UStructuralPowerMachineWireListener::BindSubsystem(AStructuralPowerGraphSubsystem* Graph,
                                                        AFGBuildable* Host) {
  if (!IsValid(Graph) || !IsValid(Host)) {
    return;
  }

  GraphSubsystem = Graph;
  BoundHost = Host;

  if (BoundConns.Num() > 0) {
    return;
  }

  TInlineComponentArray<UFGPowerConnectionComponent*> PowerConns;
  Host->GetComponents(PowerConns);
  for (UFGPowerConnectionComponent* Conn : PowerConns) {
    if (!IsValid(Conn) || Conn->IsHidden()) {
      continue;
    }

    Conn->OnConnectionChanged.AddWeakLambda(
        this,
        [this](UFGCircuitConnectionComponent* Connection) { HandleConnectionChanged(Connection); });
    BoundConns.Add(Conn);
  }
}

void UStructuralPowerMachineWireListener::EndPlay(const EEndPlayReason::Type EndPlayReason) {
  for (const TWeakObjectPtr<UFGCircuitConnectionComponent>& WeakConn : BoundConns) {
    if (UFGCircuitConnectionComponent* Conn = WeakConn.Get()) {
      Conn->OnConnectionChanged.RemoveAll(this);
    }
  }
  BoundConns.Reset();

  Super::EndPlay(EndPlayReason);
}

void UStructuralPowerMachineWireListener::HandleConnectionChanged(
    UFGCircuitConnectionComponent* /*Connection*/) {
  AStructuralPowerGraphSubsystem* Graph = GraphSubsystem.Get();
  AFGBuildable* Host = BoundHost.Get();
  if (!IsValid(Graph) || !IsValid(Host) || !Host->HasAuthority()) {
    return;
  }

  if (Graph->ShouldDeferCircuitDrivenRefresh() || Graph->IsBuildablePlacementPending(Host)) {
    return;
  }

  const IStructuralEndpointProcessor* Classified = FStructuralEndpointCatalog::Get().Classify(Host);
  if (!Classified) {
    return;
  }

  IStructuralEndpointProcessor* Processor =
      FStructuralEndpointCatalog::Get().FindMutable(Classified->GetBuildableKind());
  if (!Processor) {
    return;
  }

  FStructuralEndpointDispatcher::RunWireDelta(Graph->GetGraphSession(), *Processor, Host,
                                              EAttachContext::WireDelta);
}
