// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class UWorld;

namespace FStructuralPowerWorldGate {
STRUCTURALPOWER_API bool IsMenuWorld(const UWorld* World);

STRUCTURALPOWER_API bool IsGameplayWorld(const UWorld* World);
}
