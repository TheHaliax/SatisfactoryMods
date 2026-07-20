// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class AFGBuildableLightsControlPanel;
class FStructuralPowerBridgeProcessor;
struct FStructuralPowerContext;

class STRUCTURALPOWER_API FStructuralPowerPanelProcessor {
  friend class FStructuralPowerBridgeProcessor;

 public:
  static void Process(FStructuralPowerContext& Ctx, AFGBuildableLightsControlPanel* Panel,
                      bool bLocalPromoteOnly = false);

  static void TearDown(FStructuralPowerContext& Ctx, AFGBuildableLightsControlPanel* Panel);

  static void OnWireDelta(FStructuralPowerContext& Ctx, AFGBuildableLightsControlPanel* Panel);
};
