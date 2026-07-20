// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Graph/FStructuralPoleWireUtil.h"

#include "Buildables/FGBuildablePowerPole.h"
#include "Buildables/FGBuildableWire.h"
#include "FGPowerConnectionComponent.h"

bool FStructuralPoleWireUtil::HasVanillaWire(AFGBuildablePowerPole* Pole) {
  if (!IsValid(Pole)) {
    return false;
  }

  for (UFGPowerConnectionComponent* Visible : Pole->GetPowerConnections()) {
    if (!IsValid(Visible) || Visible->IsHidden()) {
      continue;
    }

    TArray<AFGBuildableWire*> Wires;
    Visible->GetWires(Wires);
    if (Wires.Num() > 0) {
      return true;
    }
  }

  return false;
}
