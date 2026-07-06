// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Routing/EStructuralChannel.h"

class AFGBuildable;
class AFGBuildableCircuitSwitch;

/** DR-008 / DR-011 predicates — keyed hidden links when category toggles allow. */
class STRUCTURALPOWER_API FStructuralPowerRouter
{
public:
	static bool UsesSourceControlModel(EStructuralChannel Tag);

	static bool IsReservedSentinel(FName Id);

	static bool IsPlayerChosenIdValid(FName Id);

	static bool ShouldRouteChannelLink(
		const FStructuralChannelKey& A,
		const FStructuralChannelKey& B,
		const FStructuralComponentKey& ComponentA,
		const FStructuralComponentKey& ComponentB);

	static bool ShouldRouteSwitchGate(
		FName SwitchControl,
		FName DeviceSource,
		const FStructuralComponentKey& ComponentA,
		const FStructuralComponentKey& ComponentB);

	static FName ResolveSwitchControlFromTag(const AFGBuildableCircuitSwitch* Switch);

	static FName MakeDefaultIdName(const FStructuralNodeId& CanonicalNodeId);
};
