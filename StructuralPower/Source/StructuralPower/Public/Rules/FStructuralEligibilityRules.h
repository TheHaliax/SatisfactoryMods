// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Routing/EStructuralChannel.h"

class AFGBuildable;
class AFGBuildablePowerPole;
class AFGBuildableFactoryBuilding;

/** Which buildables participate in the structural power bus. */
class STRUCTURALPOWER_API FStructuralEligibilityRules
{
public:
	static bool IsBusMember(const AFGBuildable* Buildable);
	static bool IsWallOutlet(const AFGBuildable* Buildable);
	/** Wall outlets, ground poles, and power towers that bridge visible wires to the structural bus. */
	static bool IsPowerBridgePole(const AFGBuildable* Buildable);
	/** Power circuit switch (excludes lights control panel — not AFGBuildableCircuitSwitch). */
	static bool IsPowerBridgeSwitch(const AFGBuildable* Buildable);
	static bool IsValidOutletParent(const AFGBuildable* Parent);
	static EStructuralChannel ClassifyBuildable(const AFGBuildable* Buildable);
};
