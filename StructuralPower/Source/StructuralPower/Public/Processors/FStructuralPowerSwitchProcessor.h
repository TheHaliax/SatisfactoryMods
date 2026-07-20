// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "Core/EAttachContext.h"
#include "Core/FStructuralNodeId.h"
#include "CoreMinimal.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Lightweight/FStructuralLightweightTypes.h"

class AFGBuildableCircuitSwitch;
struct FStructuralPowerContext;
class FStructuralGraphSession;

/** Thin ToggleBridge shell — kind policy lives on Attach strategies + TransferGate. */
class STRUCTURALPOWER_API FStructuralPowerSwitchProcessor {
 public:
  static void Process(FStructuralPowerContext& Ctx, AFGBuildableCircuitSwitch* Switch);

  static void ApplyStructureMembership(FStructuralPowerContext& Ctx,
                                       AFGBuildableCircuitSwitch* Switch);

  static void OnStateChanged(FStructuralPowerContext& Ctx, AFGBuildableCircuitSwitch* Switch);

  static void TearDown(FStructuralPowerContext& Ctx, AFGBuildableCircuitSwitch* Switch);

  static void OnCircuitsRebuilt(FStructuralPowerContext& Ctx, AFGBuildableCircuitSwitch* Switch);

  static void OnWireDelta(FStructuralPowerContext& Ctx, AFGBuildableCircuitSwitch* Switch);

  static void EnsureListener(FStructuralPowerContext& Ctx, AFGBuildableCircuitSwitch* Switch);

 private:
  static int32 ResolveMountRoot(FStructuralPowerContext& Ctx, const FStructuralWallAnchor& Anchor,
                                FStructuralNodeId& OutParentId);

  static int32 ResolveToggleSiteRoot(FStructuralPowerContext& Ctx,
                                     AFGBuildableCircuitSwitch* Switch, FTrackedEndpoint& Tracked);
};
