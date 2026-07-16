// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class AFGBuildablePowerPole;
class FStructuralGraphSession;
struct FStructuralNodeId;
struct FStructuralOutletParentResolveResult;
struct FStructuralPowerContext;

class STRUCTURALPOWER_API FStructuralPowerPoleProcessor {
 public:
  static void Process(FStructuralPowerContext& Ctx, AFGBuildablePowerPole* Pole);

  static void OnWireDelta(FStructuralPowerContext& Ctx, AFGBuildablePowerPole* Pole);

 private:
  static void ResolvePoleStructuralSite(
      FStructuralGraphSession& Session, AFGBuildablePowerPole* Pole, FStructuralNodeId& OutParentId,
      int32& OutRoot, bool& bStructurallyAnchored,
      FStructuralOutletParentResolveResult* OutParentResolve = nullptr);
};
