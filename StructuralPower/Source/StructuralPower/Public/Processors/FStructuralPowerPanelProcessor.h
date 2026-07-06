// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class AFGBuildableLightsControlPanel;
class AStructuralPowerGraphSubsystem;

class STRUCTURALPOWER_API FStructuralPowerPanelProcessor
{
public:
	static void Process(
		AStructuralPowerGraphSubsystem& Graph,
		AFGBuildableLightsControlPanel* Panel,
		bool bLocalPromoteOnly = false);

	static void RestitchOnRoot(AStructuralPowerGraphSubsystem& Graph, int32 Root);

	static void RestitchWithControlOnRoot(
		AStructuralPowerGraphSubsystem& Graph,
		int32 Root,
		FName ControlId);

	static void TearDown(AStructuralPowerGraphSubsystem& Graph, AFGBuildableLightsControlPanel* Panel);
};
