// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

namespace FStructuralPowerTraceUtil {
inline int32 GetFrameNumber() {
  if (GFrameCounter != 0) {
    return static_cast<int32>(GFrameCounter);
  }

  return GFrameNumber;
}
}
