// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class AFGBuildableGenerator;
struct FStructuralPowerContext;

class STRUCTURALPOWER_API FStructuralPowerGeneratorProcessor
{
public:
	static void Process(FStructuralPowerContext& Ctx, AFGBuildableGenerator* Generator);

	static void TearDown(FStructuralPowerContext& Ctx, AFGBuildableGenerator* Generator);
};
