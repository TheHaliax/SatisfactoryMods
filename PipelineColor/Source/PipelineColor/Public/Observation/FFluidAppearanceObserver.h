// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class AFGBuildable;
class AFGBuildablePipeline;
class AFGBuildablePipelinePump;
class AFGPipeNetwork;

class FFluidAppearanceObserver {
 public:
  static void RegisterHooks();
  static void EnqueueFromWorld(class UWorld* World, AFGBuildable* Buildable);
};
