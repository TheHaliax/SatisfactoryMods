// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class AFGBuildableLightsControlPanel;
struct FStructuralPowerContext;

class STRUCTURALPOWER_API FStructuralPowerPanelProcessor
{
public:
	static void Process(
		FStructuralPowerContext& Ctx,
		AFGBuildableLightsControlPanel* Panel,
		bool bLocalPromoteOnly = false);

	static void RestitchOnRoot(FStructuralPowerContext& Ctx, int32 Root);

	static void RestitchWithControlOnRoot(
		FStructuralPowerContext& Ctx,
		int32 Root,
		FName ControlId);

	static void TearDown(FStructuralPowerContext& Ctx, AFGBuildableLightsControlPanel* Panel);
};
