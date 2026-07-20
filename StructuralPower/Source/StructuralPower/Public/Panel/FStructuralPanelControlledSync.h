// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class AFGBuildableLightsControlPanel;
class FStructuralGraphSession;

struct FStructuralPanelControlledSync {
  static void ApplyKeyedSubnet(FStructuralGraphSession& Session,
                               AFGBuildableLightsControlPanel* Panel);

  static void MirrorSharedControlState(FStructuralGraphSession& Session,
                                       AFGBuildableLightsControlPanel* Panel);

  static void ReleaseIntegratedSubnet(FStructuralGraphSession& Session,
                                      AFGBuildableLightsControlPanel* Panel);

  static FName ResolveEffectiveLightControl(FStructuralGraphSession& Session,
                                            AFGBuildableLightsControlPanel* Panel);
};
