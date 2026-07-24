// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class AFGBuildableFactory;
class AFGPlayerController;

class REMOTESTANDBY_API FRSViewTarget {
 public:
  static constexpr float TraceDistanceCm = 10000.0f;

  static bool PickFactory(AFGPlayerController* PlayerController, AFGBuildableFactory*& OutFactory);
};
