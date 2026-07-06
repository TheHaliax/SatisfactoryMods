// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Lightweight/FStructuralLightweightTypes.h"

class AFGBuildableCircuitSwitch;
class FStructuralConnectivityGraph;
class FStructuralLightweightIndex;
class UWorld;

struct FStructuralSwitchParentResolveResult
{
	FStructuralWallAnchor Anchor;
	/** 0/1 when resolved from a wired port; INDEX_NONE for mount/scan. */
	int32 WirePortIndex = INDEX_NONE;

	bool IsValid() const { return Anchor.IsValid(); }
};

/** Switch mount + wire-port parent resolution (DR-001 outlet bus; ports stay grid-only). */
class STRUCTURALPOWER_API FStructuralSwitchParentResolver
{
public:
	static FStructuralSwitchParentResolveResult Resolve(
		AFGBuildableCircuitSwitch* Switch,
		UWorld* World,
		const FStructuralConnectivityGraph& Graph,
		const FStructuralLightweightIndex& LightweightIndex,
		bool bPreferWirePort = false);

	/** True when a wire port connects to a non-grid structure-side neighbor (DR-012 wired path). */
	static bool IsWiredToStructureSide(
		AFGBuildableCircuitSwitch* Switch,
		int32* OutWirePortIndex = nullptr);
};
