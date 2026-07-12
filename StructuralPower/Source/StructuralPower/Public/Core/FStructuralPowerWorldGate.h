// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class UWorld;

namespace FStructuralPowerWorldGate
{
/** Title / main-menu maps — no graph work. */
STRUCTURALPOWER_API bool IsMenuWorld(const UWorld* World);

/** Playable session map (not menu). */
STRUCTURALPOWER_API bool IsGameplayWorld(const UWorld* World);
}
