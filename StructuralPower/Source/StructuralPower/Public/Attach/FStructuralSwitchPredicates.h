// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class AFGBuildableCircuitSwitch;
struct FStructuralPowerContext;

/** Shared Control / wire / path predicates for ToggleBridge + TransferGate. */
class STRUCTURALPOWER_API FStructuralSwitchPredicates
{
public:
	static bool HasAssignedControl(
		const FStructuralPowerContext& Ctx,
		const AFGBuildableCircuitSwitch* Switch);

	static bool NeedsAdvancedWork(
		const FStructuralPowerContext& Ctx,
		const AFGBuildableCircuitSwitch* Switch);

	static bool ShouldInjectStructuralPath(const AFGBuildableCircuitSwitch* Switch);

	static uint8 BuildWireSignature(AFGBuildableCircuitSwitch* Switch);
};
