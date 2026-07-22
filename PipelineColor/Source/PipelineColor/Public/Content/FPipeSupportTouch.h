// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class AFGBuildable;
class AFGBuildablePipeline;

namespace FPipeSupportTouch {
bool IsPipeSupport(const AFGBuildable* Buildable);

// One-pass world seed: resolves every support -> pipe link up front so
// per-pipe Collect calls never fall back to world iteration. Idempotent per
// world; call from the WorldReady scan before the first paint drain.
void SeedFromWorld(UWorld* World);

void CollectSupportsTouchingPipe(AFGBuildablePipeline* Pipe, TArray<AFGBuildable*>& OutSupports);

AFGBuildablePipeline* FindTouchedPipe(AFGBuildable* Support);

void RememberLink(AFGBuildablePipeline* Pipe, AFGBuildable* Support);
void InvalidateBuildable(AFGBuildable* Buildable);
} // namespace FPipeSupportTouch
