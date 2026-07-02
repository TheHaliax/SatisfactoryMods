// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class UWorld;

/** Post-rebuild audit buckets. */
class STRUCTURALPOWER_API FStructuralPowerDiagnostics
{
public:
	static void AuditWorld(UWorld* World);
};
