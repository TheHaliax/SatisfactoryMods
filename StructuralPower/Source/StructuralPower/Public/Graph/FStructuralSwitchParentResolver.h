// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Graph/FStructuralOutletParentResolver.h"
#include "Lightweight/FStructuralLightweightIndex.h"

struct FStructuralSwitchParentResolveResult
{
	FStructuralWallAnchor Anchor;
	int32 WirePortIndex = INDEX_NONE;

	bool IsValid() const { return Anchor.IsValid(); }
};

class STRUCTURALPOWER_API FStructuralSwitchParentResolver
{
public:
	static FStructuralSwitchParentResolveResult Resolve(
		AFGBuildableCircuitSwitch* Switch,
		UWorld* World,
		const FStructuralConnectivityGraph& Graph,
		const FStructuralLightweightIndex& LightweightIndex,
		bool bPreferWirePort = false,
		const FStructuralOutletParentResolveParams* ParentResolveParams = nullptr);

	static bool IsWiredToStructureSide(
		AFGBuildableCircuitSwitch* Switch,
		int32* OutWirePortIndex = nullptr);

	static void ForEachWiredStructureSideAnchor(
		AFGBuildableCircuitSwitch* Switch,
		UWorld* World,
		const FStructuralLightweightIndex& LightweightIndex,
		const FStructuralOutletParentResolveParams* ParentResolveParams,
		TFunctionRef<void(const FStructuralWallAnchor& Anchor)> Visitor);
};
