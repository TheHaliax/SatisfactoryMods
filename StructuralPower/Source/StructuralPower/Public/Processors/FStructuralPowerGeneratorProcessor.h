// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class AFGBuildableGenerator;
struct FStructuralPowerContext;

/** AFGBuildableGenerator host — no-op until router enables M7 routing. PRE-2.2 Part II. */
class STRUCTURALPOWER_API FStructuralPowerGeneratorProcessor
{
public:
	static void Process(FStructuralPowerContext& Ctx, AFGBuildableGenerator* Generator);

	static void RestitchOnRoot(FStructuralPowerContext& Ctx, int32 Root);

	static void TearDown(FStructuralPowerContext& Ctx, AFGBuildableGenerator* Generator);
};
