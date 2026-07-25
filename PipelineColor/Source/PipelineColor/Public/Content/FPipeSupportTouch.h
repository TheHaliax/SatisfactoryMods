// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class AFGBuildable;
class AFGBuildablePipeline;

namespace FPipeSupportTouch {
bool IsPipeSupport(const AFGBuildable* Buildable);

// One-pass seed: wire support→pipe up front so Collect never world-iterates.
void SeedFromWorld(UWorld* World);

void CollectSupportsTouchingPipe(AFGBuildablePipeline* Pipe, TArray<AFGBuildable*>& OutSupports);

AFGBuildablePipeline* FindTouchedPipe(AFGBuildable* Support);

void RememberLink(AFGBuildablePipeline* Pipe, AFGBuildable* Support);
void InvalidateBuildable(AFGBuildable* Buildable);
} // namespace FPipeSupportTouch
