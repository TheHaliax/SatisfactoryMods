// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Core/EAttachContext.h"

class AFGBuildableCircuitSwitch;
class UFGPowerConnectionComponent;
struct FStructuralPowerContext;

class STRUCTURALPOWER_API FStructuralPowerTransferGate
{
public:
	static bool IsBridgeWireOrToggleContext(EAttachContext AttachContext);

	static bool IsConsumerVanillaWired(const UFGPowerConnectionComponent* VisiblePlug);

	static void FlipBridgeGate(
		FStructuralPowerContext& Ctx,
		AFGBuildableCircuitSwitch* Switch,
		bool bGateOpen);

	static void ApplyKeyedTransferOnRoot(
		FStructuralPowerContext& Ctx,
		int32 Root,
		FName SwitchControlId,
		bool bGateOpen,
		bool bLocalPromoteOnly = true);

	static void RefreshKeyedTransferOnRoot(FStructuralPowerContext& Ctx, int32 Root);
};
