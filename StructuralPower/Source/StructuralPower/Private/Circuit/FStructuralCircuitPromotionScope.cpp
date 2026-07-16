// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Circuit/FStructuralCircuitPromotionScope.h"

#include "Save/AStructuralPowerGraphSubsystem.h"

FStructuralCircuitPromotionScope::FStructuralCircuitPromotionScope(
    AStructuralPowerGraphSubsystem* InGraph)
    : Graph(InGraph) {
  if (Graph) {
    Graph->BeginCircuitPromotion();
  }
}

FStructuralCircuitPromotionScope::~FStructuralCircuitPromotionScope() {
  if (Graph) {
    Graph->EndCircuitPromotion();
  }
}
