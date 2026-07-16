// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class AFGBuildable;
class AFGBuildableGenerator;
struct FStructuralPowerContext;

class STRUCTURALPOWER_API FStructuralPowerGeneratorProcessor {
 public:
  static void Process(FStructuralPowerContext& Ctx, AFGBuildableGenerator* Generator);
  static void ProcessFactoryHost(FStructuralPowerContext& Ctx, AFGBuildable* Host);

  static void TearDown(FStructuralPowerContext& Ctx, AFGBuildableGenerator* Generator);
  static void TearDownFactoryHost(FStructuralPowerContext& Ctx, AFGBuildable* Host);
};
