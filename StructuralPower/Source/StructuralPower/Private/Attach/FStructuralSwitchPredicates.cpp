// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Attach/FStructuralSwitchPredicates.h"

#include "Attach/FStructuralSwitchWireEcho.h"
#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Core/FStructuralPowerContext.h"
#include "Graph/FStructuralSwitchParentResolver.h"
#include "Routing/EStructuralChannel.h"
#include "Routing/FStructuralPowerRouter.h"
#include "Core/FStructuralGraphSession.h"
#include "Save/FStructuralGraphIdOps.h"

bool FStructuralSwitchPredicates::HasAssignedControl(
	const FStructuralPowerContext& Ctx,
	const AFGBuildableCircuitSwitch* Switch)
{
	if (!IsValid(Switch))
	{
		return false;
	}

	const FName Control = const_cast<FStructuralPowerContext&>(Ctx).Session().Ids().ResolveControl(
		const_cast<AFGBuildableCircuitSwitch*>(Switch),
		EStructuralChannel::Switch);
	return FStructuralPowerRouter::IsAssignedControl(Control);
}

bool FStructuralSwitchPredicates::NeedsAdvancedWork(
	const FStructuralPowerContext& Ctx,
	const AFGBuildableCircuitSwitch* Switch)
{
	if (!IsValid(Switch))
	{
		return false;
	}

	return HasAssignedControl(Ctx, Switch)
		|| FStructuralSwitchParentResolver::HasAnyVanillaWire(
			const_cast<AFGBuildableCircuitSwitch*>(Switch));
}

bool FStructuralSwitchPredicates::ShouldInjectStructuralPath(const AFGBuildableCircuitSwitch* Switch)
{
	return IsValid(Switch) && Switch->IsBridgeActive();
}

uint8 FStructuralSwitchPredicates::BuildWireSignature(AFGBuildableCircuitSwitch* Switch)
{
	return FStructuralSwitchWireEcho::BuildWireSignature(Switch);
}
