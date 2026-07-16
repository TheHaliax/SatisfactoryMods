// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class UWorld;

class STRUCTURALPOWER_API FStructuralPowerDiagnostics {
 public:
  static void AuditWorld(UWorld* World, bool bAllowMenuWorld = false);
};
