// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

enum class EStructuralPowerRole : uint8 {
  Gateway,
  Router,
  Host,
};

STRUCTURALPOWER_API const TCHAR* StructuralPowerRoleToString(EStructuralPowerRole Role);
