// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Core/EAttachContext.h"
#include "Core/FStructuralNodeId.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Lightweight/FStructuralLightweightTypes.h"

class AFGBuildable;
class AFGBuildableCircuitSwitch;
class AFGBuildableLightSource;
class AFGBuildableLightsControlPanel;
struct FStructuralPowerContext;
class UFGStructuralPowerConnectionComponent;

class STRUCTURALPOWER_API FStructuralPowerSwitchProcessor
{
public:
	static void Process(FStructuralPowerContext& Ctx, AFGBuildableCircuitSwitch* Switch);

	static void OnStateChanged(FStructuralPowerContext& Ctx, AFGBuildableCircuitSwitch* Switch);

	static void TearDown(FStructuralPowerContext& Ctx, AFGBuildable* Host);

	static void StripInactiveLinksOnRoot(FStructuralPowerContext& Ctx, int32 Root);

	static void RestitchKeyedConsumersOnRoot(
		FStructuralPowerContext& Ctx,
		int32 Root,
		FName SwitchControlId,
		bool bLocalPromoteOnly = false);

	static void RestitchActiveKeyedConsumersOnRoot(FStructuralPowerContext& Ctx, int32 Root);

	static void PropagateWiredFeedChange(
		FStructuralPowerContext& Ctx,
		AFGBuildableCircuitSwitch* Switch,
		int32 LocalRoot);

	static bool HasAssignedControl(
		const FStructuralPowerContext& Ctx,
		const AFGBuildableCircuitSwitch* Switch);

	static bool NeedsAdvancedWork(
		const FStructuralPowerContext& Ctx,
		const AFGBuildableCircuitSwitch* Switch);

	static bool ShouldInjectStructuralPath(const AFGBuildableCircuitSwitch* Switch);

private:
	static int32 ResolveMountRoot(
		FStructuralPowerContext& Ctx,
		const FStructuralWallAnchor& Anchor,
		FStructuralNodeId& OutParentId);

	static void EnsureListener(FStructuralPowerContext& Ctx, AFGBuildableCircuitSwitch* Switch);

	static void RegisterOutletBase(
		FStructuralPowerContext& Ctx,
		AFGBuildableCircuitSwitch* Switch,
		const FStructuralWallAnchor& ParentAnchor,
		FTrackedEndpoint& InOutTracked,
		int32& OutRoot,
		FStructuralNodeId& OutParentId);

	static void ApplyBaseOutletAttach(
		FStructuralPowerContext& Ctx,
		AFGBuildableCircuitSwitch* Switch,
		UFGStructuralPowerConnectionComponent* OutletBus,
		int32 Root);

	static void ApplyRuntimeAttach(
		FStructuralPowerContext& Ctx,
		AFGBuildableCircuitSwitch* Switch,
		UFGStructuralPowerConnectionComponent* OutletBus,
		int32 Root,
		const FStructuralNodeId& SwitchId,
		EAttachContext AttachContext);

	static void ApplyAdvancedAttach(
		FStructuralPowerContext& Ctx,
		AFGBuildableCircuitSwitch* Switch,
		UFGStructuralPowerConnectionComponent* OutletBus,
		int32 Root,
		const FStructuralNodeId& SwitchId,
		bool bKeyedSubnet);

	static void RestitchKeyedSubnet(
		FStructuralPowerContext& Ctx,
		AFGBuildableCircuitSwitch* Switch,
		UFGStructuralPowerConnectionComponent* OutletBus,
		int32 ComponentRoot,
		const FStructuralNodeId& SwitchNodeId);

	static void LogConsumerRestitchSummary(
		FStructuralPowerContext& Ctx,
		AFGBuildableCircuitSwitch* Switch,
		int32 Root,
		FName SwitchControlId,
		bool bSwitchOn);
};
