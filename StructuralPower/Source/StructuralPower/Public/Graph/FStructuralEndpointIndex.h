// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "Core/FStructuralNodeId.h"
#include "CoreMinimal.h"
#include "Graph/FStructuralEndpointTypes.h"

class FStructuralConnectivityGraph;

class FStructuralEndpointIndex {
 public:
  void Reset();

  void RebuildFrom(const TMap<FStructuralNodeId, FTrackedEndpoint>& Tracked,
                   const FStructuralConnectivityGraph& Graph);

  void Add(int32 Root, EStructuralEndpointKind Kind, const FStructuralNodeId& EndpointId);

  void RemoveRoot(int32 Root);

  const TArray<FStructuralNodeId>* Get(int32 Root, EStructuralEndpointKind Kind) const;

  void ForEachBridgeOnRoot(int32 Root,
                           TFunctionRef<void(const FStructuralNodeId& EndpointId)> Visitor) const;

  void ForEachOnRoot(int32 Root,
                     TFunctionRef<void(const FStructuralNodeId& EndpointId)> Visitor) const;

  void ForEachEndpoint(EStructuralEndpointKind Kind,
                       TFunctionRef<void(const FStructuralNodeId& EndpointId)> Visitor) const;

 private:
  TMap<int32, TMap<EStructuralEndpointKind, TArray<FStructuralNodeId>>> ByRoot;
};
