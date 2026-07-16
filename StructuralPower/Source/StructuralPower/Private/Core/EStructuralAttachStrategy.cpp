// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Core/EStructuralAttachStrategy.h"

const TCHAR* StructuralAttachStrategyToString(const EStructuralAttachStrategy Strategy) {
  switch (Strategy) {
    case EStructuralAttachStrategy::Bridge:
      return TEXT("bridge");
    case EStructuralAttachStrategy::ToggleBridge:
      return TEXT("toggle_bridge");
    case EStructuralAttachStrategy::Consumer:
      return TEXT("consumer");
    case EStructuralAttachStrategy::Router:
      return TEXT("router");
    default:
      return TEXT("unknown");
  }
}
