// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Core/FPCWorldGate.h"

#include "Engine/World.h"

namespace FPCWorldGate {
bool IsMenuWorld(const UWorld* World) {
  if (!IsValid(World)) {
    return true;
  }

  const FString MapName = World->GetMapName();
  return MapName.Contains(TEXT("Menu"), ESearchCase::IgnoreCase);
}

bool IsGameplayWorld(const UWorld* World) {
  return IsValid(World) && World->IsGameWorld() && !IsMenuWorld(World);
}
}
