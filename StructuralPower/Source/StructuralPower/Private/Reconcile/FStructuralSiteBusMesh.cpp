// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Reconcile/FStructuralSiteBusMesh.h"

#include "Buildables/FGBuildable.h"
#include "Components/UFGStructuralPowerConnectionComponent.h"
#include "FGCircuitConnectionComponent.h"
#include "FGPowerConnectionComponent.h"
#include "Graph/FStructuralEndpointIndex.h"
#include "Graph/FStructuralEndpointTypes.h"

void FStructuralSiteBusMesh::Remesh(
    int32 Root, bool bTearDownFirst, const FStructuralEndpointIndex& EndpointIndex,
    TFunctionRef<const FTrackedEndpoint*(const FStructuralNodeId&)> FindTracked,
    TFunctionRef<bool(AFGBuildable*, EStructuralEndpointKind)> ShouldParticipate,
    TFunctionRef<UFGStructuralPowerConnectionComponent*(AFGBuildable*)> GetOrCreateBus,
    TFunctionRef<void(AFGBuildable*, UFGStructuralPowerConnectionComponent*)> LinkBusToVisible,
    TFunctionRef<bool(AFGBuildable*, AFGBuildable*, int32)> ShouldMeshEndpoints,
    TFunctionRef<bool(UFGPowerConnectionComponent*, UFGPowerConnectionComponent*)> LinkHiddenPair,
    TFunctionRef<bool(const UFGPowerConnectionComponent*)> ComponentCarriesPower,
    TFunctionRef<UFGStructuralPowerConnectionComponent*(UFGStructuralPowerConnectionComponent*)>
        FindPoweredHiddenReachable,
    TFunctionRef<void(UFGStructuralPowerConnectionComponent*)> PromoteStructuralMeshFrom) {
  if (Root == INDEX_NONE) {
    return;
  }

  TArray<AFGBuildable*> Hosts;
  TArray<UFGStructuralPowerConnectionComponent*> Buses;

  EndpointIndex.ForEachOnRoot(Root, [&](const FStructuralNodeId& NodeId) {
    const FTrackedEndpoint* Tracked = FindTracked(NodeId);
    if (!Tracked) {
      return;
    }

    AFGBuildable* Host = Tracked->Actor.Get();
    if (!IsValid(Host)) {
      return;
    }

    if (!ShouldParticipate(Host, Tracked->Kind)) {
      return;
    }

    UFGStructuralPowerConnectionComponent* Bus = GetOrCreateBus(Host);
    if (!Bus) {
      return;
    }

    Hosts.Add(Host);
    Buses.Add(Bus);
  });

  if (Buses.Num() == 0) {
    return;
  }

  if (bTearDownFirst) {
    for (UFGStructuralPowerConnectionComponent* Bus : Buses) {
      TArray<UFGCircuitConnectionComponent*> HiddenLinks;
      Bus->GetHiddenConnections(HiddenLinks);
      for (UFGCircuitConnectionComponent* OtherRaw : HiddenLinks) {
        if (UFGStructuralPowerConnectionComponent* OtherBus =
                Cast<UFGStructuralPowerConnectionComponent>(OtherRaw)) {
          Bus->RemoveHiddenConnection(OtherBus);
        }
      }
    }
  }

  UFGStructuralPowerConnectionComponent* Anchor = Buses[0];
  for (int32 Index = 0; Index < Buses.Num(); ++Index) {
    LinkBusToVisible(Hosts[Index], Buses[Index]);
    if (Index > 0 && ShouldMeshEndpoints(Hosts[0], Hosts[Index], Root)) {
      LinkHiddenPair(Buses[Index], Anchor);
    }
  }

  UFGStructuralPowerConnectionComponent* Seed = nullptr;
  for (UFGStructuralPowerConnectionComponent* Bus : Buses) {
    if (ComponentCarriesPower(Bus)) {
      Seed = Bus;
      break;
    }
  }

  if (!Seed) {
    for (UFGStructuralPowerConnectionComponent* Bus : Buses) {
      if (UFGStructuralPowerConnectionComponent* Reachable = FindPoweredHiddenReachable(Bus)) {
        Seed = Reachable;
        break;
      }
    }
  }

  if (Seed) {
    PromoteStructuralMeshFrom(Seed);
  }
}
