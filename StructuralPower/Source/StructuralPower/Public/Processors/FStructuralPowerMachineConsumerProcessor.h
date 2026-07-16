// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Graph/FStructuralEndpointTypes.h"

class AFGBuildable;
struct FStructuralPowerContext;

class FStructuralGraphSession;

/** Control-role machine attach (extractors / mfg / transport / pumps) — light pattern. */
class STRUCTURALPOWER_API FStructuralPowerMachineConsumerProcessor {
 public:
  static void Process(FStructuralPowerContext& Ctx, AFGBuildable* Device,
                      EStructuralEndpointKind Kind, bool (*IsGroupEnabled)());

  static void TearDown(FStructuralPowerContext& Ctx, AFGBuildable* Device);

  /** After a source joins a root bus — revive consumers that failed power gate. */
  static void EnqueueInactiveConsumersOnRoot(FStructuralGraphSession& Session, int32 Root);
};
