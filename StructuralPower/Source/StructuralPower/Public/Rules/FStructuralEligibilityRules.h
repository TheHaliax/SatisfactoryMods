// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Routing/EStructuralChannel.h"

class AFGBuildable;
class AFGBuildableFactoryBuilding;

class STRUCTURALPOWER_API FStructuralEligibilityRules
{
public:
	static bool IsBusMember(const AFGBuildable* Buildable);
	static bool IsPowerBridgePole(const AFGBuildable* Buildable);
	static bool IsPowerBridgeSwitch(const AFGBuildable* Buildable);
	static bool IsPowerStorage(const AFGBuildable* Buildable);
	static bool IsStructuralLightConsumer(const AFGBuildable* Buildable);
	static bool IsStructuralGenerator(const AFGBuildable* Buildable);
	static bool IsIdConfigTarget(const AFGBuildable* Buildable);
	static bool IsValidOutletParent(const AFGBuildable* Parent);
	static EStructuralChannel ClassifyBuildable(const AFGBuildable* Buildable);
};
