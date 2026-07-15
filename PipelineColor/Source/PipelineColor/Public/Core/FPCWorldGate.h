// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class UWorld;

namespace FPCWorldGate
{
bool IsMenuWorld(const UWorld* World);
bool IsGameplayWorld(const UWorld* World);
}
