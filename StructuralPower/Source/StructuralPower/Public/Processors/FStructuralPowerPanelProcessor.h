// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

#include "Core/EAttachContext.h"

class AFGBuildableLightsControlPanel;
class AStructuralPowerGraphSubsystem;

/** AFGBuildableLightsControlPanel host — PRE-2.2 Part II role=host. */
class STRUCTURALPOWER_API FStructuralPowerPanelProcessor
{
public:
	static void Process(
		AStructuralPowerGraphSubsystem& Graph,
		AFGBuildableLightsControlPanel* Panel,
		bool bLocalPromoteOnly = false);

	static void RestitchOnRoot(
		AStructuralPowerGraphSubsystem& Graph,
		int32 Root,
		EAttachContext AttachContext);

	static void RestitchWithControlOnRoot(
		AStructuralPowerGraphSubsystem& Graph,
		int32 Root,
		FName ControlId,
		EAttachContext AttachContext);

	static void TearDown(AStructuralPowerGraphSubsystem& Graph, AFGBuildableLightsControlPanel* Panel);
};
