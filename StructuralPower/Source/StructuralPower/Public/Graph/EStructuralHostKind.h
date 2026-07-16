// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

enum class EStructuralHostKind : uint8 {
  Unknown,
  Foundation,
  Wall,
  FactoryBuilding,
  Lightweight,
  GenericBusMember
};
