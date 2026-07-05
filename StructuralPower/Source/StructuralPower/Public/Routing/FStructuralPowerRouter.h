// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Routing/EStructuralChannel.h"

class AFGBuildable;
class AFGBuildableCircuitSwitch;

/** DR-008 predicates — inert until category toggles enable keyed hidden links. */
class STRUCTURALPOWER_API FStructuralPowerRouter
{
public:
	static bool ShouldRouteChannelLink(
		const FStructuralChannelKey& A,
		const FStructuralChannelKey& B,
		const FStructuralComponentKey& ComponentA,
		const FStructuralComponentKey& ComponentB);

	static bool ShouldRouteSwitchGate(
		FName SwitchEffectiveId,
		FName DeviceEffectiveId,
		const FStructuralComponentKey& ComponentA,
		const FStructuralComponentKey& ComponentB);

	static FName ResolveSwitchEffectiveId(const AFGBuildableCircuitSwitch* Switch);

	static FName MakeDefaultIdName(const FStructuralNodeId& CanonicalNodeId);
};
