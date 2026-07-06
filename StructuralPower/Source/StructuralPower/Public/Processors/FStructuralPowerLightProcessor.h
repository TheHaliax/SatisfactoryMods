// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class AFGBuildableLightSource;
struct FStructuralPowerContext;

/** AFGBuildableLightSource host — PRE-2.2 Part II role=host. */
class STRUCTURALPOWER_API FStructuralPowerLightProcessor
{
public:
	static void Process(
		FStructuralPowerContext& Ctx,
		AFGBuildableLightSource* Light,
		bool bLocalPromoteOnly = false);

	static void RestitchOnRoot(FStructuralPowerContext& Ctx, int32 Root);

	static void TearDown(FStructuralPowerContext& Ctx, AFGBuildableLightSource* Light);
};
