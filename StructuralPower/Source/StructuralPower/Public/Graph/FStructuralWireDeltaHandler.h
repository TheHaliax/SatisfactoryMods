// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class AFGBuildableCircuitSwitch;
class AFGBuildableLightsControlPanel;
class AFGBuildablePowerPole;
class AStructuralPowerGraphSubsystem;

class STRUCTURALPOWER_API FStructuralWireDeltaHandler
{
public:
	FStructuralWireDeltaHandler() = default;

	void Bind(AStructuralPowerGraphSubsystem* InSubsystem);

	void ProcessSwitchWireDelta(AFGBuildableCircuitSwitch* Switch);
	void ProcessSwitchCircuitsRebuilt(AFGBuildableCircuitSwitch* Switch);
	void ProcessPanelWireDelta(AFGBuildableLightsControlPanel* Panel);
	void ProcessPoleWireDelta(AFGBuildablePowerPole* Pole);
	void ProcessWallOutletAfterWire(AFGBuildablePowerPole* Pole);

private:
	AStructuralPowerGraphSubsystem* Subsystem = nullptr;
};
