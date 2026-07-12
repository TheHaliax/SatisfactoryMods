// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Processors/FStructuralSwitchBridgeStrategy.h"

#include "Processors/FStructuralPowerSwitchProcessor.h"

void FStructuralSwitchBridgeStrategy::Apply(
	FStructuralPowerContext& Ctx,
	AFGBuildableCircuitSwitch* Switch)
{
	FStructuralPowerSwitchProcessor::ApplySwitchBridgeStrategy(Ctx, Switch);
}

void FStructuralSwitchBridgeStrategy::DisarmDirectedPair(
	FStructuralPowerContext& Ctx,
	AFGBuildableCircuitSwitch* Switch)
{
	FStructuralPowerSwitchProcessor::DisarmDirectedPair(Ctx, Switch);
}

void FStructuralSwitchBridgeStrategy::SyncDirectedBridgePair(
	FStructuralPowerContext& Ctx,
	AFGBuildableCircuitSwitch* Switch)
{
	FStructuralPowerSwitchProcessor::SyncDirectedBridgePair(Ctx, Switch);
}
