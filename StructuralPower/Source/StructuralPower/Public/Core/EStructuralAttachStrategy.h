// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

enum class EStructuralAttachStrategy : uint8 {
  Bridge,
  ToggleBridge,
  Consumer,
  Router,
};

STRUCTURALPOWER_API const TCHAR* StructuralAttachStrategyToString(
    EStructuralAttachStrategy Strategy);
