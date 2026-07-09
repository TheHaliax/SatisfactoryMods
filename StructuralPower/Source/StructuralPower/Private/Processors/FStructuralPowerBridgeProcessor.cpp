// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Processors/FStructuralPowerBridgeProcessor.h"

#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Buildables/FGBuildableLightsControlPanel.h"
#include "Config/FStructuralPowerModConfig.h"
#include "Core/EAttachContext.h"
#include "Core/EStructuralPowerRole.h"
#include "Core/EStructuralPowerScope.h"
#include "Graph/FStructuralCrossSiteGraph.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Core/FStructuralPowerContext.h"
#include "Connection/FStructuralSwitchConnectionPoint.h"
#include "Processors/FStructuralPowerPanelProcessor.h"
#include "Save/AStructuralPowerGraphSubsystem.h"
#include "StructuralPowerLog.h"

void FStructuralPowerBridgeProcessor::PropagateCrossSiteFeedChange(
	FStructuralPowerContext& Ctx,
	AFGBuildableCircuitSwitch* Switch,
	int32 LocalRoot)
{
	if (!IsValid(Switch) || LocalRoot == INDEX_NONE)
	{
		return;
	}

	Ctx.Graph().CrossSiteGraph.RefreshCouplingsFromWiredSwitch(Ctx.Graph(), Switch, LocalRoot);

	TArray<int32> AffectedRoots;
	Ctx.Graph().CrossSiteGraph.TraceFeedAffected(
		Ctx.Graph(),
		Switch,
		LocalRoot,
		AffectedRoots);

	if (AffectedRoots.Num() == 0)
	{
		return;
	}

	UE_LOG(LogStructuralPower, Log,
		TEXT("[HALSP] wired switch %s kind=%s scope=%s site=%d role=%s path=wire_bridge"
			" localRoot=%d feedAffected=%d"),
		*Switch->GetName(),
		StructuralEndpointKindToString(EStructuralEndpointKind::Switch),
		StructuralPowerScopeToString(EStructuralPowerScope::CrossSite),
		LocalRoot,
		StructuralPowerRoleToString(EStructuralPowerRole::Gateway),
		LocalRoot,
		AffectedRoots.Num());
}

void FStructuralPowerBridgeProcessor::ApplyLocalAttachForSwitch(
	FStructuralPowerContext& Ctx,
	AFGBuildableCircuitSwitch* Switch,
	UFGStructuralPowerConnectionComponent* /*OutletBus*/,
	int32 /*Root*/,
	const FStructuralNodeId& /*SwitchNodeId*/,
	EAttachContext AttachContext,
	bool /*bKeyedSubnet*/)
{
	if (!IsValid(Switch))
	{
		return;
	}

	FStructuralSwitchConnectionPoint(Ctx.Graph(), Switch).OnWireOrGateChanged(AttachContext);
}

void FStructuralPowerBridgeProcessor::ApplyLocalAttachForPanel(
	FStructuralPowerContext& Ctx,
	AFGBuildableLightsControlPanel* Panel,
	bool bLocalPromoteOnly)
{
	if (!IsValid(Panel) || !FStructuralPowerModConfig::IsGroupLightingEnabled())
	{
		return;
	}

	FStructuralPowerPanelProcessor::Process(Ctx, Panel, bLocalPromoteOnly);
}
