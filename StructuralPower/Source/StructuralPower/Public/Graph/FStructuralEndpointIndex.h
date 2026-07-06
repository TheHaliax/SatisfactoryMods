// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Core/FStructuralNodeId.h"
#include "Graph/FStructuralEndpointTypes.h"

class FStructuralConnectivityGraph;

/** Root → kind → endpoint ids. Rebuilt from tracked endpoints on load; incremental during bulk drain. */
class FStructuralEndpointIndex
{
public:
	void Reset();

	void RebuildFrom(
		const TMap<FStructuralNodeId, FTrackedEndpoint>& Tracked,
		const FStructuralConnectivityGraph& Graph);

	void Add(int32 Root, EStructuralEndpointKind Kind, const FStructuralNodeId& EndpointId);

	const TArray<FStructuralNodeId>* Get(int32 Root, EStructuralEndpointKind Kind) const;

	void ForEachBridgeOnRoot(
		int32 Root,
		TFunctionRef<void(const FStructuralNodeId& EndpointId)> Visitor) const;

	void ForEachOnRoot(
		int32 Root,
		TFunctionRef<void(const FStructuralNodeId& EndpointId)> Visitor) const;

private:
	TMap<int32, TMap<EStructuralEndpointKind, TArray<FStructuralNodeId>>> ByRoot;
};
