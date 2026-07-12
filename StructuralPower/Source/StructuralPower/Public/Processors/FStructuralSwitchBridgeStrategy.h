// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class AFGBuildableCircuitSwitch;
struct FStructuralPowerContext;

class STRUCTURALPOWER_API FStructuralSwitchBridgeStrategy
{
public:
	static void Apply(FStructuralPowerContext& Ctx, AFGBuildableCircuitSwitch* Switch);
	static void DisarmDirectedPair(FStructuralPowerContext& Ctx, AFGBuildableCircuitSwitch* Switch);
	static void SyncDirectedBridgePair(FStructuralPowerContext& Ctx, AFGBuildableCircuitSwitch* Switch);
};
