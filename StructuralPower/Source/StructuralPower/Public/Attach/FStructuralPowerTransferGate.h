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

	static void RefreshKeyedTransferOnRoot(
		FStructuralPowerContext& Ctx,
		int32 Root,
		bool bLocalPromoteOnly = true);

	static void RefreshSiteStructuralConsumersOnRoot(
		FStructuralPowerContext& Ctx,
		int32 Root,
		bool bGateOpen);

	static void SuspendAllKeyedLightingTransfer(FStructuralPowerContext& Ctx);

	static void PropagateWiredFeedChange(
		FStructuralPowerContext& Ctx,
		AFGBuildableCircuitSwitch* Switch,
		int32 LocalRoot);

	static void ApplyWireDeltaTransferSideEffects(
		FStructuralPowerContext& Ctx,
		AFGBuildableCircuitSwitch* Switch,
		int32 Root);

	static void ApplyToggleTransferSideEffects(
		FStructuralPowerContext& Ctx,
		AFGBuildableCircuitSwitch* Switch,
		int32 Root,
		bool bKeyedSubnet,
		FName SwitchControlId,
		bool bSwitchOn);
};
