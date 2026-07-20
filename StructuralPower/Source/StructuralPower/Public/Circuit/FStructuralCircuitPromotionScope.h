// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class AStructuralPowerGraphSubsystem;

struct FStructuralCircuitPromotionScope {
  AStructuralPowerGraphSubsystem* Graph = nullptr;

  explicit FStructuralCircuitPromotionScope(AStructuralPowerGraphSubsystem* InGraph);

  ~FStructuralCircuitPromotionScope();

  FStructuralCircuitPromotionScope(const FStructuralCircuitPromotionScope&) = delete;
  FStructuralCircuitPromotionScope& operator=(const FStructuralCircuitPromotionScope&) = delete;
};
