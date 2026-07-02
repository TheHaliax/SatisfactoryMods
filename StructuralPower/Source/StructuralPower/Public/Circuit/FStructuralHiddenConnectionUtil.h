// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class UFGCircuitConnectionComponent;

/** Bidirectional mesh-only hidden links without RebuildCircuit storm. */
class STRUCTURALPOWER_API FStructuralHiddenConnectionUtil
{
public:
	static bool AddMeshOnlyHiddenLink(UFGCircuitConnectionComponent* A, UFGCircuitConnectionComponent* B);
};
