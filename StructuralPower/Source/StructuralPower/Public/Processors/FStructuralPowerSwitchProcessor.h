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
class AStructuralPowerGraphSubsystem;
class UFGStructuralPowerConnectionComponent;

class STRUCTURALPOWER_API FStructuralPowerSwitchProcessor
{
public:
	static void Process(AStructuralPowerGraphSubsystem& Graph, AFGBuildableCircuitSwitch* Switch);

	static void OnStateChanged(AStructuralPowerGraphSubsystem& Graph, AFGBuildableCircuitSwitch* Switch);

	static void TearDown(AStructuralPowerGraphSubsystem& Graph, AFGBuildable* Host);

	static void StripInactiveLinksOnRoot(AStructuralPowerGraphSubsystem& Graph, int32 Root);

	static void RestitchKeyedConsumersOnRoot(
		AStructuralPowerGraphSubsystem& Graph,
		int32 Root,
		FName SwitchControlId,
		bool bLocalPromoteOnly = false);

	static void RestitchActiveKeyedConsumersOnRoot(AStructuralPowerGraphSubsystem& Graph, int32 Root);

	static void PropagateWiredFeedChange(
		AStructuralPowerGraphSubsystem& Graph,
		AFGBuildableCircuitSwitch* Switch,
		int32 LocalRoot);

	static bool HasAssignedControl(
		const AStructuralPowerGraphSubsystem& Graph,
		const AFGBuildableCircuitSwitch* Switch);

	static bool NeedsAdvancedWork(
		const AStructuralPowerGraphSubsystem& Graph,
		const AFGBuildableCircuitSwitch* Switch);

	static bool ShouldInjectStructuralPath(const AFGBuildableCircuitSwitch* Switch);

private:
	static int32 ResolveMountRoot(
		AStructuralPowerGraphSubsystem& Graph,
		const FStructuralWallAnchor& Anchor,
		FStructuralNodeId& OutParentId);

	static void EnsureListener(AStructuralPowerGraphSubsystem& Graph, AFGBuildableCircuitSwitch* Switch);

	static void RegisterOutletBase(
		AStructuralPowerGraphSubsystem& Graph,
		AFGBuildableCircuitSwitch* Switch,
		const FStructuralWallAnchor& ParentAnchor,
		FTrackedEndpoint& InOutTracked,
		int32& OutRoot,
		FStructuralNodeId& OutParentId);

	static void ApplyBaseOutletAttach(
		AStructuralPowerGraphSubsystem& Graph,
		AFGBuildableCircuitSwitch* Switch,
		UFGStructuralPowerConnectionComponent* OutletBus,
		int32 Root);

	static void ApplyRuntimeAttach(
		AStructuralPowerGraphSubsystem& Graph,
		AFGBuildableCircuitSwitch* Switch,
		UFGStructuralPowerConnectionComponent* OutletBus,
		int32 Root,
		const FStructuralNodeId& SwitchId,
		EAttachContext AttachContext);

	static void ApplyAdvancedAttach(
		AStructuralPowerGraphSubsystem& Graph,
		AFGBuildableCircuitSwitch* Switch,
		UFGStructuralPowerConnectionComponent* OutletBus,
		int32 Root,
		const FStructuralNodeId& SwitchId,
		bool bKeyedSubnet);

	static void RestitchKeyedSubnet(
		AStructuralPowerGraphSubsystem& Graph,
		AFGBuildableCircuitSwitch* Switch,
		UFGStructuralPowerConnectionComponent* OutletBus,
		int32 ComponentRoot,
		const FStructuralNodeId& SwitchNodeId);

	static void LogConsumerRestitchSummary(
		AStructuralPowerGraphSubsystem& Graph,
		AFGBuildableCircuitSwitch* Switch,
		int32 Root,
		FName SwitchControlId,
		bool bSwitchOn);
};
