// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Graph/FStructuralPowerBuildableCasts.h"

#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Buildables/FGBuildableLightSource.h"
#include "Buildables/FGBuildableLightsControlPanel.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "Buildables/FGBuildablePowerStorage.h"

AFGBuildableCircuitSwitch* FStructuralPowerBuildableCasts::AsSwitch(AFGBuildable* Buildable) {
  return Cast<AFGBuildableCircuitSwitch>(Buildable);
}

AFGBuildablePowerPole* FStructuralPowerBuildableCasts::AsPole(AFGBuildable* Buildable) {
  return Cast<AFGBuildablePowerPole>(Buildable);
}

AFGBuildableLightSource* FStructuralPowerBuildableCasts::AsLight(AFGBuildable* Buildable) {
  return Cast<AFGBuildableLightSource>(Buildable);
}

AFGBuildableLightsControlPanel* FStructuralPowerBuildableCasts::AsPanel(AFGBuildable* Buildable) {
  return Cast<AFGBuildableLightsControlPanel>(Buildable);
}

AFGBuildablePowerStorage* FStructuralPowerBuildableCasts::AsStorage(AFGBuildable* Buildable) {
  return Cast<AFGBuildablePowerStorage>(Buildable);
}
