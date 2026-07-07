// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Core/EAttachContext.h"
#include "Core/FStructuralNodeId.h"

class AFGBuildableCircuitSwitch;
class AFGBuildableLightsControlPanel;
class UFGStructuralPowerConnectionComponent;
struct FStructuralPowerContext;

/** Unified CircuitBridge attach — local pair gate only; no site remesh on wire/toggle. */
class STRUCTURALPOWER_API FStructuralPowerBridgeProcessor
{
public:
	static void PropagateCrossSiteFeedChange(
		FStructuralPowerContext& Ctx,
		AFGBuildableCircuitSwitch* Switch,
		int32 LocalRoot);

	static void ApplyLocalAttachForSwitch(
		FStructuralPowerContext& Ctx,
		AFGBuildableCircuitSwitch* Switch,
		UFGStructuralPowerConnectionComponent* OutletBus,
		int32 Root,
		const FStructuralNodeId& SwitchNodeId,
		EAttachContext AttachContext,
		bool bKeyedSubnet);

	static void FinishPanelBridgeLegsOnSiteAfterGateChange(
		FStructuralPowerContext& Ctx,
		int32 SiteRoot);

	static void ApplyLocalAttachForPanel(
		FStructuralPowerContext& Ctx,
		AFGBuildableLightsControlPanel* Panel,
		bool bLocalPromoteOnly);

private:
	static void ApplyBaseOutletAttach(
		FStructuralPowerContext& Ctx,
		AFGBuildableCircuitSwitch* Switch,
		UFGStructuralPowerConnectionComponent* OutletBus,
		int32 Root);

	static void ApplyAdvancedAttach(
		FStructuralPowerContext& Ctx,
		AFGBuildableCircuitSwitch* Switch,
		UFGStructuralPowerConnectionComponent* OutletBus,
		int32 Root,
		const FStructuralNodeId& SwitchId,
		bool bKeyedSubnet);
};
