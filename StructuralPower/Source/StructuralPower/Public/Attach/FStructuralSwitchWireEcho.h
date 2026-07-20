// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class AFGBuildableCircuitSwitch;
struct FStructuralPowerContext;

class STRUCTURALPOWER_API FStructuralSwitchWireEcho {
 public:
  static uint8 BuildWireSignature(AFGBuildableCircuitSwitch* Switch);
  static void OnCircuitsRebuilt(FStructuralPowerContext& Ctx, AFGBuildableCircuitSwitch* Switch);
};
