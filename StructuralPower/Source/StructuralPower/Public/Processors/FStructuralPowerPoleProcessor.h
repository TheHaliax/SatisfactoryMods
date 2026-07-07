// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class AFGBuildablePowerPole;
struct FStructuralPowerContext;

class STRUCTURALPOWER_API FStructuralPowerPoleProcessor
{
public:
	static void Process(FStructuralPowerContext& Ctx, AFGBuildablePowerPole* Pole);
};
