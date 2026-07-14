// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Graph/FStructuralEndpointTypes.h"

class AFGBuildable;
class FStructuralEndpointIndex;
class UFGPowerConnectionComponent;
class UFGStructuralPowerConnectionComponent;
struct FTrackedEndpoint;

class STRUCTURALPOWER_API FStructuralSiteBusMesh
{
public:
	static void Remesh(
		int32 Root,
		bool bTearDownFirst,
		const FStructuralEndpointIndex& EndpointIndex,
		TFunctionRef<const FTrackedEndpoint*(const FStructuralNodeId&)> FindTracked,
		TFunctionRef<bool(AFGBuildable*, EStructuralEndpointKind)> ShouldParticipate,
		TFunctionRef<UFGStructuralPowerConnectionComponent*(AFGBuildable*)> GetOrCreateBus,
		TFunctionRef<void(AFGBuildable*, UFGStructuralPowerConnectionComponent*)> LinkBusToVisible,
		TFunctionRef<bool(AFGBuildable*, AFGBuildable*, int32)> ShouldMeshEndpoints,
		TFunctionRef<bool(UFGPowerConnectionComponent*, UFGPowerConnectionComponent*)> LinkHiddenPair,
		TFunctionRef<bool(const UFGPowerConnectionComponent*)> ComponentCarriesPower,
		TFunctionRef<UFGStructuralPowerConnectionComponent*(UFGStructuralPowerConnectionComponent*)>
			FindPoweredHiddenReachable,
		TFunctionRef<void(UFGStructuralPowerConnectionComponent*)> PromoteStructuralMeshFrom);
};
