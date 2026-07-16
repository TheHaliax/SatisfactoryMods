// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class AFGBuildable;
class AFGBuildablePipeline;

// Soft-IsA fluid support parents only — bare PolePipe is shared with hypertube.
namespace FPipeSupportTouch {
bool IsPipeSupport(const AFGBuildable* Buildable);

void CollectSupportsTouchingPipe(AFGBuildablePipeline* Pipe, TArray<AFGBuildable*>& OutSupports);

AFGBuildablePipeline* FindTouchedPipe(AFGBuildable* Support);

void RememberLink(AFGBuildablePipeline* Pipe, AFGBuildable* Support);
void InvalidateBuildable(AFGBuildable* Buildable);
}  // namespace FPipeSupportTouch
