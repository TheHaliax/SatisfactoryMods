// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Network/UStructuralPowerSwitchListener.h"

#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Core/FStructuralPowerContext.h"
#include "Processors/FStructuralPowerSwitchProcessor.h"
#include "Save/AStructuralPowerGraphSubsystem.h"

namespace {
static bool SwitchNeedsToggleSubscription(AStructuralPowerGraphSubsystem& Graph,
                                          AFGBuildableCircuitSwitch* Switch) {
  if (!IsValid(Switch)) {
    return false;
  }

  return true;
}
}  // namespace

void UStructuralPowerSwitchListener::BindSubsystem(AStructuralPowerGraphSubsystem* Graph,
                                                   AFGBuildableCircuitSwitch* Switch) {
  if (!IsValid(Graph) || !IsValid(Switch)) {
    return;
  }

  GraphSubsystem = Graph;
  BoundSwitch = Switch;
  SyncSubscriptions(Graph, Switch);
}

void UStructuralPowerSwitchListener::SyncSubscriptions(AStructuralPowerGraphSubsystem* Graph,
                                                       AFGBuildableCircuitSwitch* Switch) {
  if (!IsValid(Graph) || !IsValid(Switch)) {
    return;
  }

  const bool bNeedsToggle = SwitchNeedsToggleSubscription(*Graph, Switch);

  if (bToggleBound && !bNeedsToggle) {
    Switch->mOnIsSwitchOnChanged.RemoveDynamic(
        this, &UStructuralPowerSwitchListener::HandleSwitchOnChanged);
    bToggleBound = false;
  } else if (!bToggleBound && bNeedsToggle) {
    Switch->mOnIsSwitchOnChanged.AddDynamic(this,
                                            &UStructuralPowerSwitchListener::HandleSwitchOnChanged);
    bToggleBound = true;
  }
}

void UStructuralPowerSwitchListener::EndPlay(const EEndPlayReason::Type EndPlayReason) {
  if (AFGBuildableCircuitSwitch* Switch = BoundSwitch.Get()) {
    if (bToggleBound) {
      Switch->mOnIsSwitchOnChanged.RemoveDynamic(
          this, &UStructuralPowerSwitchListener::HandleSwitchOnChanged);
    }
  }

  bToggleBound = false;

  Super::EndPlay(EndPlayReason);
}

void UStructuralPowerSwitchListener::HandleSwitchOnChanged() {
  if (AStructuralPowerGraphSubsystem* Graph = GraphSubsystem.Get()) {
    Graph->OnSwitchStateChanged(BoundSwitch.Get());
  }
}
