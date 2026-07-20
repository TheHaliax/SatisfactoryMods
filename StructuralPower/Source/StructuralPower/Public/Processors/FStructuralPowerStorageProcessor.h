// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class AFGBuildablePowerStorage;
struct FStructuralPowerContext;

class STRUCTURALPOWER_API FStructuralPowerStorageProcessor {
 public:
  static void Process(FStructuralPowerContext& Ctx, AFGBuildablePowerStorage* Storage);

  static void OnWireDelta(FStructuralPowerContext& Ctx, AFGBuildablePowerStorage* Storage);

  static void TearDown(FStructuralPowerContext& Ctx, AFGBuildablePowerStorage* Storage);
};
