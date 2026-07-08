// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class AFGBuildable;
class AFGBuildableCircuitSwitch;
class AFGBuildableLightSource;
class AFGBuildableLightsControlPanel;
class AFGBuildablePowerPole;
class AFGBuildablePowerStorage;

struct STRUCTURALPOWER_API FStructuralPowerBuildableCasts
{
	static AFGBuildableCircuitSwitch* AsSwitch(AFGBuildable* Buildable);
	static AFGBuildablePowerPole* AsPole(AFGBuildable* Buildable);
	static AFGBuildableLightSource* AsLight(AFGBuildable* Buildable);
	static AFGBuildableLightsControlPanel* AsPanel(AFGBuildable* Buildable);
	static AFGBuildablePowerStorage* AsStorage(AFGBuildable* Buildable);
};
