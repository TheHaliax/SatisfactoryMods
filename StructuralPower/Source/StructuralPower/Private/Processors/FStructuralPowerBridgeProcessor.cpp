// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Processors/FStructuralPowerBridgeProcessor.h"

#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Circuit/FStructuralCircuitPromotionUtil.h"
#include "Components/UFGStructuralPowerConnectionComponent.h"
#include "Config/FStructuralPowerModConfig.h"
#include "Core/EAttachContext.h"
#include "Core/EStructuralPowerRole.h"
#include "Core/EStructuralPowerScope.h"
#include "FGCircuitConnectionComponent.h"
#include "FGPowerConnectionComponent.h"
#include "Graph/FStructuralCrossSiteGraph.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Graph/FStructuralPowerBuildableCasts.h"
#include "Graph/FStructuralSwitchParentResolver.h"
#include "Core/FStructuralPowerContext.h"
#include "Connection/FStructuralPanelConnectionPoint.h"
#include "Processors/FStructuralPowerPanelProcessor.h"
#include "Save/AStructuralPowerGraphSubsystem.h"
#include "StructuralPowerLog.h"

void FStructuralPowerBridgeProcessor::ApplyBaseOutletAttach(
	FStructuralPowerContext& Ctx,
	AFGBuildableCircuitSwitch* Switch,
	UFGStructuralPowerConnectionComponent* OutletBus,
	int32 Root)
{
	if (!IsValid(Switch) || !IsValid(OutletBus) || Root == INDEX_NONE)
	{
		return;
	}

	Ctx.Graph().LinkBusToVisibleConnectionsLocal(Switch, OutletBus);

	if (!Ctx.Graph().DoesComponentRootCarryPower(Root))
	{
		return;
	}

	if (UFGCircuitConnectionComponent* Feed = Ctx.Graph().GetComponentSourceConnector(Root, Switch))
	{
		if (UFGPowerConnectionComponent* FeedPower = Cast<UFGPowerConnectionComponent>(Feed))
		{
			Ctx.Graph().LinkHiddenPairLocal(OutletBus, FeedPower);
		}
	}

	Ctx.Graph().PromoteOutletBusIfPowered(OutletBus, /*bLocalPromoteOnly=*/true);
}

void FStructuralPowerBridgeProcessor::ApplyAdvancedAttach(
	FStructuralPowerContext& Ctx,
	AFGBuildableCircuitSwitch* Switch,
	UFGStructuralPowerConnectionComponent* OutletBus,
	int32 Root,
	const FStructuralNodeId& SwitchId,
	bool bKeyedSubnet)
{
	if (!IsValid(Switch) || !IsValid(OutletBus))
	{
		return;
	}

	Ctx.Graph().LinkBusToVisibleConnectionsLocal(Switch, OutletBus);

	if (Root == INDEX_NONE)
	{
		Ctx.Graph().PromoteOutletBusIfPowered(OutletBus, /*bLocalPromoteOnly=*/true);
		return;
	}

	UFGPowerConnectionComponent* FeedPower = nullptr;
	if (UFGCircuitConnectionComponent* Conn0 = Switch->GetConnection0())
	{
		if (UFGPowerConnectionComponent* Visible = Cast<UFGPowerConnectionComponent>(Conn0))
		{
			if (IsValid(Visible) && FStructuralCircuitPromotionUtil::ComponentCarriesPower(Visible))
			{
				FeedPower = Visible;
			}
		}
	}
	if (!FeedPower)
	{
		if (UFGCircuitConnectionComponent* Conn1 = Switch->GetConnection1())
		{
			if (UFGPowerConnectionComponent* Visible = Cast<UFGPowerConnectionComponent>(Conn1))
			{
				if (IsValid(Visible) && FStructuralCircuitPromotionUtil::ComponentCarriesPower(Visible))
				{
					FeedPower = Visible;
				}
			}
		}
	}
	if (!FeedPower)
	{
		if (UFGCircuitConnectionComponent* Feed = Ctx.Graph().GetComponentSourceConnector(Root, Switch))
		{
			FeedPower = Cast<UFGPowerConnectionComponent>(Feed);
		}
	}

	if (FeedPower)
	{
		Ctx.Graph().LinkHiddenPairLocal(OutletBus, FeedPower);
	}

	const bool bWiredBridge = FStructuralSwitchParentResolver::IsWiredToStructureSide(Switch);
	if (bKeyedSubnet && !Ctx.Graph().HasBridgeBusPeerMesh(OutletBus))
	{
		Ctx.Graph().TryMeshPeerBusOnComponent(
			Switch,
			OutletBus,
			Root,
			SwitchId,
			/*bBridgePeersOnly=*/false);
	}
	else if (bWiredBridge && !Ctx.Graph().HasBridgeBusPeerMesh(OutletBus))
	{
		Ctx.Graph().TryMeshPeerBusOnComponent(
			Switch,
			OutletBus,
			Root,
			SwitchId,
			/*bBridgePeersOnly=*/true);
	}

	Ctx.Graph().PromoteOutletBusIfPowered(OutletBus, /*bLocalPromoteOnly=*/true);
}

void FStructuralPowerBridgeProcessor::PropagateCrossSiteFeedChange(
	FStructuralPowerContext& Ctx,
	AFGBuildableCircuitSwitch* Switch,
	int32 LocalRoot)
{
	if (!IsValid(Switch) || LocalRoot == INDEX_NONE)
	{
		return;
	}

	if (FStructuralPowerModConfig::IsPowerSwitchManualGroupsEnabled()
		&& Ctx.GetAttachContext() == EAttachContext::Toggle)
	{
		return;
	}

	Ctx.Graph().CrossSiteGraph.RefreshCouplingsFromWiredSwitch(Ctx.Graph(), Switch, LocalRoot);

	TArray<int32> AffectedRoots;
	Ctx.Graph().CrossSiteGraph.TraceFeedAffected(Ctx.Graph(), Switch, LocalRoot, AffectedRoots);
	if (AffectedRoots.Num() == 0)
	{
		return;
	}

	const EAttachContext AttachContext = Ctx.GetAttachContext();
	TSet<int32> Done;
	for (int32 Root : AffectedRoots)
	{
		if (Root == INDEX_NONE || Done.Contains(Root))
		{
			continue;
		}
		Done.Add(Root);
		Ctx.Graph().RestitchKeyedSubnetsAfterMeshFeedRefresh(Root, AttachContext);
	}

	UE_LOG(LogStructuralPower, Log,
		TEXT("[HALSP] wired switch %s scope=%s site=%d role=%s path=wire_bridge"
			" localRoot=%d feedAffected=%d"),
		*Switch->GetName(),
		StructuralPowerScopeToString(EStructuralPowerScope::CrossSite),
		LocalRoot,
		StructuralPowerRoleToString(EStructuralPowerRole::Gateway),
		LocalRoot,
		AffectedRoots.Num());
}

void FStructuralPowerBridgeProcessor::ApplyLocalAttachForSwitch(
	FStructuralPowerContext& Ctx,
	AFGBuildableCircuitSwitch* Switch,
	UFGStructuralPowerConnectionComponent* OutletBus,
	int32 Root,
	const FStructuralNodeId& SwitchNodeId,
	EAttachContext AttachContext,
	bool bKeyedSubnet)
{
	if (!IsValid(Switch) || !IsValid(OutletBus))
	{
		return;
	}

	if (IsBulkLoadAttachContext(AttachContext))
	{
		Ctx.Graph().LinkBusToVisibleConnections(Switch, OutletBus);
		if (Root != INDEX_NONE)
		{
			Ctx.Graph().ApplyLocalBridgeBusAttach(Switch, OutletBus, Root, SwitchNodeId, Switch);
		}
		else
		{
			Ctx.Graph().PromoteOutletBusIfPowered(OutletBus, /*bLocalPromoteOnly=*/false);
		}
		return;
	}

	const bool bWiredBridge = FStructuralSwitchParentResolver::IsWiredToStructureSide(Switch);
	if (bKeyedSubnet || bWiredBridge)
	{
		ApplyAdvancedAttach(Ctx, Switch, OutletBus, Root, SwitchNodeId, bKeyedSubnet);
	}
	else
	{
		ApplyBaseOutletAttach(Ctx, Switch, OutletBus, Root);
	}
}

void FStructuralPowerBridgeProcessor::FinishPanelBridgeLegsOnSiteAfterGateChange(
	FStructuralPowerContext& Ctx,
	int32 SiteRoot)
{
	if (SiteRoot == INDEX_NONE
		|| !FStructuralPowerModConfig::IsGroupLightingEnabled()
		|| Ctx.GetAttachContext() != EAttachContext::Toggle)
	{
		return;
	}

	Ctx.Graph().RefreshBridgeEndpointRootIndex();

	const TArray<FStructuralNodeId>* PanelIds =
		Ctx.Graph().EndpointIndex.Get(SiteRoot, EStructuralEndpointKind::Panel);
	if (!PanelIds || PanelIds->Num() == 0)
	{
		return;
	}

	const TArray<FStructuralNodeId> PanelIdsSnapshot = *PanelIds;
	for (const FStructuralNodeId& PanelId : PanelIdsSnapshot)
	{
		const FTrackedEndpoint* Tracked = Ctx.Graph().TrackedEndpoints.Find(PanelId);
		if (!Tracked)
		{
			continue;
		}

		AFGBuildableLightsControlPanel* Panel = Tracked->GetPanel();
		if (!IsValid(Panel))
		{
			continue;
		}

		FStructuralPanelConnectionPoint Connection(Ctx.Graph(), Panel);
		Connection.OnWireOrGateChanged(EAttachContext::Toggle);
	}
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
