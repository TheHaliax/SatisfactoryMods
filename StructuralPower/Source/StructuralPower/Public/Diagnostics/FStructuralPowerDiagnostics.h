// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class UWorld;

/** Logs a snapshot of the structural graph and bridge-pole circuit state. */
class STRUCTURALPOWER_API FStructuralPowerDiagnostics
{
public:
	/** bAllowMenuWorld=false skips menu maps during automatic post-load audits. */
	static void AuditWorld(UWorld* World, bool bAllowMenuWorld = false);
};
