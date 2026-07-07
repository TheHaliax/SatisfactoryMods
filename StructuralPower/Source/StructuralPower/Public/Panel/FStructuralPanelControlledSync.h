// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class AFGBuildableLightsControlPanel;
class AStructuralPowerGraphSubsystem;

struct FStructuralPanelControlledSync
{
	static void ApplyKeyedSubnet(
		AStructuralPowerGraphSubsystem& Graph,
		AFGBuildableLightsControlPanel* Panel);
};
