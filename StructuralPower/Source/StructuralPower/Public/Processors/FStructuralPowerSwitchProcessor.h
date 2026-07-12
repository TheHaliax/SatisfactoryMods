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
class AStructuralPowerGraphSubsystem;
class UFGStructuralPowerConnectionComponent;

class STRUCTURALPOWER_API FStructuralPowerSwitchProcessor
{
public:
	static void Process(FStructuralPowerContext& Ctx, AFGBuildableCircuitSwitch* Switch);

	static void ApplyStructureMembership(
		FStructuralPowerContext& Ctx,
		AFGBuildableCircuitSwitch* Switch);

	static void OnStateChanged(FStructuralPowerContext& Ctx, AFGBuildableCircuitSwitch* Switch);

	static void TearDown(FStructuralPowerContext& Ctx, AFGBuildableCircuitSwitch* Switch);

	static void DisarmDirectedPair(FStructuralPowerContext& Ctx, AFGBuildableCircuitSwitch* Switch);

	static void SyncDirectedBridgePair(
		FStructuralPowerContext& Ctx,
		AFGBuildableCircuitSwitch* Switch);

	static void ApplySwitchBridgeStrategy(
		FStructuralPowerContext& Ctx,
		AFGBuildableCircuitSwitch* Switch);

	static void OnCircuitsRebuilt(
		FStructuralPowerContext& Ctx,
		AFGBuildableCircuitSwitch* Switch);

	static uint8 BuildWireSignature(AFGBuildableCircuitSwitch* Switch);

	static void RemeshUnmeshedPeersAfterBulkLoad(FStructuralPowerContext& Ctx);

	static void PropagateWiredFeedChange(
		FStructuralPowerContext& Ctx,
		AFGBuildableCircuitSwitch* Switch,
		int32 LocalRoot);

	static void ApplyWireDeltaTransferSideEffects(
		FStructuralPowerContext& Ctx,
		AFGBuildableCircuitSwitch* Switch,
		int32 Root);

	static void OnWireDelta(FStructuralPowerContext& Ctx, AFGBuildableCircuitSwitch* Switch);

	static bool HasAssignedControl(
		const FStructuralPowerContext& Ctx,
		const AFGBuildableCircuitSwitch* Switch);

	static bool NeedsAdvancedWork(
		const FStructuralPowerContext& Ctx,
		const AFGBuildableCircuitSwitch* Switch);

	static bool ShouldInjectStructuralPath(const AFGBuildableCircuitSwitch* Switch);

	static void EnsureListener(FStructuralPowerContext& Ctx, AFGBuildableCircuitSwitch* Switch);

private:
	static int32 ResolveMountRoot(
		FStructuralPowerContext& Ctx,
		const FStructuralWallAnchor& Anchor,
		FStructuralNodeId& OutParentId);

	static int32 ResolveToggleSiteRoot(
		FStructuralPowerContext& Ctx,
		AFGBuildableCircuitSwitch* Switch,
		FTrackedEndpoint& Tracked);

	static bool ResolveSwitchTrackedRoot(
		AStructuralPowerGraphSubsystem& Graph,
		AFGBuildableCircuitSwitch* Switch,
		FStructuralNodeId& OutSwitchId,
		int32& OutRoot,
		bool& OutStructurallyAnchored);
};
