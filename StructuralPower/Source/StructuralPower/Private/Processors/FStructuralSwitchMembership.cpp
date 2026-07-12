// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Processors/FStructuralSwitchMembership.h"

#include "Processors/FStructuralPowerSwitchProcessor.h"

void FStructuralSwitchMembership::Apply(
	FStructuralPowerContext& Ctx,
	AFGBuildableCircuitSwitch* Switch)
{
	FStructuralPowerSwitchProcessor::ApplyStructureMembership(Ctx, Switch);
}
