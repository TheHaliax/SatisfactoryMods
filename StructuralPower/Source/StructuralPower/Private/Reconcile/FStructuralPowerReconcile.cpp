// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Reconcile/FStructuralPowerReconcile.h"

#include "Core/FStructuralGraphSession.h"
#include "Save/AStructuralPowerGraphSubsystem.h"

#include "Attach/FStructuralPanelAttach.h"
#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Buildables/FGBuildableLightSource.h"
#include "Buildables/FGBuildableLightsControlPanel.h"
#include "Circuit/FStructuralCircuitPromotionUtil.h"
#include "Config/FStructuralPowerModConfig.h"
#include "Diagnostics/FStructuralPowerTrace.h"
#include "FGBuildableSubsystem.h"
#include "FGPowerConnectionComponent.h"
#include "Misc/App.h"
#include "Panel/FStructuralPanelControlledSync.h"
#include "Panel/FStructuralPanelPortResolver.h"
#include "Processors/FStructuralEndpointDispatcher.h"
#include "Processors/FStructuralPowerTransferGate.h"
#include "Processors/FStructuralPowerPanelProcessor.h"
#include "Processors/FStructuralPowerLightProcessor.h"
#include "Processors/FStructuralPowerSwitchProcessor.h"
#include "Save/FStructuralControlIdGangIndex.h"
#include "Routing/FStructuralPowerRouter.h"
#include "Core/EAttachContext.h"
#include "Core/FStructuralPowerContext.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Graph/FStructuralPowerBuildableCasts.h"
#include "Rules/FStructuralEligibilityRules.h"
#include "StructuralPowerConstants.h"
#include "StructuralPowerLog.h"

void FStructuralPowerReconcile::Bind(FStructuralGraphSession* InSession)
{
	Session = InSession;
}

void FStructuralPowerReconcile::RunPostLoadLightWorkers()
{
	if (!Session || !FStructuralPowerModConfig::IsGroupLightingEnabled())
	{
		return;
	}

	ReconcileAllPanelEndpoints();
	RefreshKeyedTransferAfterLoad();
	RefreshNamedControlPanelsAfterLoad();
	Session->FinalLightingReconcilePass() = 0;
	Session->ScheduleFinalLightingReconcile();
}

void FStructuralPowerReconcile::MaybeRunFinalLightingReconcile()
{
	if (!Session->PendingFinalLightingReconcile())
	{
		return;
	}

	if (Session->GetPendingJobCount() > 0)
	{
		Session->ScheduleFinalLightingReconcile();
		return;
	}

	if (!FStructuralPowerModConfig::IsGroupLightingEnabled())
	{
		Session->PendingFinalLightingReconcile() = false;
		Session->FinalLightingReconcilePass() = 0;
		Session->MaybeReleaseFactoryTick();
		return;
	}

	++Session->FinalLightingReconcilePass();

	UE_LOG(LogStructuralPower, Log,
		TEXT("[HALSP] Final lighting reconcile pass=%d/%d — panels + keyed transfer + lights"),
		Session->FinalLightingReconcilePass(),
		AStructuralPowerGraphSubsystem::MaxFinalLightingReconcilePasses);

	ReconcileAllPanelEndpoints();
	RefreshKeyedTransferAfterLoad();
	ReconcileAllLightConsumers();

	if (LightingReconcileNeedsRetry()
		&& Session->FinalLightingReconcilePass()
			< AStructuralPowerGraphSubsystem::MaxFinalLightingReconcilePasses)
	{
		Session->ScheduleFinalLightingReconcile();
		return;
	}

	Session->FinalLightingReconcilePass() = 0;
	Session->PendingFinalLightingReconcile() = false;
	Session->MaybeReleaseFactoryTick();
}

bool FStructuralPowerReconcile::PanelNeedsLightingReconcileRetry(
	AFGBuildableLightsControlPanel* Panel) const
{
	if (!IsValid(Panel))
	{
		return false;
	}

	const FName EffectiveControl =
		FStructuralPanelControlledSync::ResolveEffectiveLightControl(*Session, Panel);
	if (EffectiveControl.IsNone())
	{
		return true;
	}

	FStructuralPanelPorts Ports;
	if (!FStructuralPanelPortResolver::Resolve(Panel, Ports))
	{
		return true;
	}

	const FStructuralChannelKey ChannelKey =
		Session->Ids().ResolveChannelKeyForBuildable(Panel);
	const int32 Root = Session->GetEndpointComponentRoot(Panel);
	if (Root == INDEX_NONE)
	{
		return true;
	}

	if (!FStructuralPanelAttach::SupplyAlreadyLinked(
			*Session, Panel, Ports, Root, ChannelKey))
	{
		return true;
	}

	const UFGPowerConnectionComponent* InputPower =
		FStructuralPanelPortResolver::AsPowerConnection(Ports.Input);
	return !FStructuralCircuitPromotionUtil::ConnectorSuppliesPower(InputPower);
}

bool FStructuralPowerReconcile::LightingReconcileNeedsRetry()
{
	if (!FStructuralPowerModConfig::IsGroupLightingEnabled())
	{
		return false;
	}

	TArray<AFGBuildableLightsControlPanel*> Panels;
	CollectKnownPanelEndpoints(Panels);

	for (AFGBuildableLightsControlPanel* Panel : Panels)
	{
		if (PanelNeedsLightingReconcileRetry(Panel))
		{
			return true;
		}
	}

	return false;
}

bool FStructuralPowerReconcile::IsDirectSwitchFedLight(
	int32 Root,
	const FStructuralChannelKey& LightKey)
{
	if (Root == INDEX_NONE || LightKey.Source.IsNone())
	{
		return false;
	}

	Session->BridgeRootIndex().RefreshBridgeEndpointRootIndex();

	if (const TArray<FStructuralNodeId>* SwitchIds =
			Session->EndpointIndex().Get(Root, EStructuralEndpointKind::Switch))
	{
		const TArray<FStructuralNodeId> SwitchIdsSnapshot = *SwitchIds;
		for (const FStructuralNodeId& NodeId : SwitchIdsSnapshot)
		{
			if (const FTrackedEndpoint* Tracked = Session->TrackedEndpoints().Find(NodeId))
			{
				if (AFGBuildableCircuitSwitch* Switch = Tracked->GetSwitch())
				{
					if (Session->Ids().ResolveControl(Switch, EStructuralChannel::Switch) == LightKey.Source)
					{
						return true;
					}
				}
			}
		}
	}

	for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : Session->TrackedEndpoints())
	{
		if (Pair.Value.Kind != EStructuralEndpointKind::Switch)
		{
			continue;
		}

		if (Session->StructureGraph().FindRoot(Pair.Value.MountParentId) != Root)
		{
			continue;
		}

		if (AFGBuildableCircuitSwitch* Switch = Pair.Value.GetSwitch())
		{
			if (Session->Ids().ResolveControl(Switch, EStructuralChannel::Switch) == LightKey.Source)
			{
				return true;
			}
		}
	}

	return false;
}

bool FStructuralPowerReconcile::IsPanelDownstreamLight(
	int32 Root,
	const FStructuralChannelKey& LightKey)
{
	if (Root == INDEX_NONE || LightKey.Source.IsNone() || IsDirectSwitchFedLight(Root, LightKey))
	{
		return false;
	}

	auto PanelHostsLightGroup = [&](AFGBuildableLightsControlPanel* Panel) -> bool
	{
		if (!IsValid(Panel))
		{
			return false;
		}

		const FStructuralWallAnchor Anchor = Session->ResolveOutletAnchor(Panel);
		FStructuralNodeId ParentId;
		if (Session->BridgeRootIndex().ResolveEndpointComponentRoot(Panel, Anchor, ParentId) != Root)
		{
			return false;
		}

		const FName PanelControl = Session->Ids().ResolveControl(Panel, EStructuralChannel::Light);
		return !PanelControl.IsNone()
			&& PanelControl != StructuralPowerConstants::ControlUnconfigured
			&& PanelControl == LightKey.Source;
	};

	Session->BridgeRootIndex().RefreshBridgeEndpointRootIndex();

	if (const TArray<FStructuralNodeId>* PanelIds =
			Session->EndpointIndex().Get(Root, EStructuralEndpointKind::Panel))
	{
		const TArray<FStructuralNodeId> PanelIdsSnapshot = *PanelIds;
		for (const FStructuralNodeId& PanelId : PanelIdsSnapshot)
		{
			if (const FTrackedEndpoint* Tracked = Session->TrackedEndpoints().Find(PanelId))
			{
				if (PanelHostsLightGroup(Tracked->GetPanel()))
				{
					return true;
				}
			}
		}
	}

	for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : Session->TrackedEndpoints())
	{
		if (Pair.Value.Kind != EStructuralEndpointKind::Panel)
		{
			continue;
		}

		if (PanelHostsLightGroup(Pair.Value.GetPanel()))
		{
			return true;
		}
	}

	bool bFoundPendingPanel = false;
	Session->Placement().ForEachPendingOutletBuildable([&](AFGBuildable* Buildable)
	{
		if (!bFoundPendingPanel
			&& PanelHostsLightGroup(FStructuralPowerBuildableCasts::AsPanel(Buildable)))
		{
			bFoundPendingPanel = true;
		}
	});

	if (bFoundPendingPanel)
	{
		return true;
	}

	// BeginPlay / seed race: panels may not be tracked yet on first post-load pass.
	if (UWorld* World = Session->GetWorld())
	{
		if (AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(World))
		{
			for (AFGBuildable* Buildable : BuildableSubsystem->GetAllBuildablesRef())
			{
				if (PanelHostsLightGroup(FStructuralPowerBuildableCasts::AsPanel(Buildable)))
				{
					return true;
				}
			}
		}
	}

	return false;
}

bool FStructuralPowerReconcile::IsSwitchFeedOpen(
	int32 Root,
	FName SwitchControlId)
{
	if (Root == INDEX_NONE || SwitchControlId.IsNone())
	{
		return false;
	}

	auto TrySwitch = [&](AFGBuildableCircuitSwitch* Switch, bool& OutMatched) -> bool
	{
		OutMatched = false;
		if (!IsValid(Switch))
		{
			return false;
		}

		if (Session->Ids().ResolveControl(Switch, EStructuralChannel::Switch) != SwitchControlId)
		{
			return false;
		}

		OutMatched = true;
		return !FStructuralPowerSwitchProcessor::ShouldInjectStructuralPath(Switch)
			|| !Switch->IsBridgeActive();
	};

	if (const TArray<FStructuralNodeId>* SwitchIds =
			Session->EndpointIndex().Get(Root, EStructuralEndpointKind::Switch))
	{
		for (const FStructuralNodeId& NodeId : *SwitchIds)
		{
			if (const FTrackedEndpoint* Tracked = Session->TrackedEndpoints().Find(NodeId))
			{
				bool bMatched = false;
				const bool bOpen = TrySwitch(Tracked->GetSwitch(), bMatched);
				if (bMatched)
				{
					return bOpen;
				}
			}
		}
	}

	// Global Control gang: switches on other sites with the same Control gate this feed.
	for (const FStructuralNodeId& NodeId :
		Session->ControlIdGangIndex().GetGlobalGangMembers(SwitchControlId))
	{
		if (const FTrackedEndpoint* Tracked = Session->TrackedEndpoints().Find(NodeId))
		{
			bool bMatched = false;
			const bool bOpen = TrySwitch(Tracked->GetSwitch(), bMatched);
			if (bMatched)
			{
				return bOpen;
			}
		}
	}

	return false;
}

void FStructuralPowerReconcile::LogPanelReconcileSummary(
	AFGBuildableLightsControlPanel* Panel)
{
	if (!IsValid(Panel))
	{
		return;
	}

	FStructuralPanelPorts Ports;
	if (!FStructuralPanelPortResolver::Resolve(Panel, Ports))
	{
		return;
	}

	const FStructuralChannelKey ChannelKey = Session->Ids().ResolveChannelKeyForBuildable(Panel);
	const FStructuralWallAnchor ParentAnchor = Session->ResolveOutletAnchor(Panel);
	FStructuralNodeId ParentId;
	const int32 Root = Session->BridgeRootIndex().ResolveEndpointComponentRoot(Panel, ParentAnchor, ParentId);
	const UFGPowerConnectionComponent* InputPower =
		FStructuralPanelPortResolver::AsPowerConnection(Ports.Input);
	const bool bSupplyReady =
		FStructuralCircuitPromotionUtil::ConnectorSuppliesPower(InputPower);
	const int32 Controlled =
		Panel->GetControlledBuildables(AFGBuildableLightSource::StaticClass()).Num();

	FStructuralPowerTrace::LogPanelConsumer(
		Panel,
		Root,
		ChannelKey,
		Ports,
		bSupplyReady,
		Controlled,
		TEXT("reconcile"));
}

void FStructuralPowerReconcile::CollectKnownPanelEndpoints(
	TArray<AFGBuildableLightsControlPanel*>& OutPanels)
{
	OutPanels.Reset();
	TSet<AFGBuildableLightsControlPanel*> Seen;

	auto ConsiderPanel = [&](AFGBuildableLightsControlPanel* Panel)
	{
		if (!IsValid(Panel) || !Panel->HasAuthority() || Seen.Contains(Panel))
		{
			return;
		}

		Seen.Add(Panel);
		OutPanels.Add(Panel);
	};

	Session->BridgeRootIndex().RefreshBridgeEndpointRootIndex();

	Session->EndpointIndex().ForEachEndpoint(EStructuralEndpointKind::Panel, [&](const FStructuralNodeId& PanelId)
	{
		if (const FTrackedEndpoint* Tracked = Session->TrackedEndpoints().Find(PanelId))
		{
			ConsiderPanel(Tracked->GetPanel());
		}
	});

	for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : Session->TrackedEndpoints())
	{
		if (Pair.Value.Kind != EStructuralEndpointKind::Panel)
		{
			continue;
		}

		ConsiderPanel(Pair.Value.GetPanel());
	}

	Session->Placement().ForEachPendingOutletBuildable([&](AFGBuildable* Buildable)
	{
		ConsiderPanel(FStructuralPowerBuildableCasts::AsPanel(Buildable));
	});

	if (Seen.Num() > 0)
	{
		return;
	}

	if (UWorld* World = Session->GetWorld())
	{
		if (AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(World))
		{
			for (AFGBuildable* Buildable : BuildableSubsystem->GetAllBuildablesRef())
			{
				ConsiderPanel(FStructuralPowerBuildableCasts::AsPanel(Buildable));
			}
		}
	}
}

void FStructuralPowerReconcile::ReconcileAllPanelEndpoints()
{
	TArray<AFGBuildableLightsControlPanel*> Panels;
	CollectKnownPanelEndpoints(Panels);

	for (AFGBuildableLightsControlPanel* Panel : Panels)
	{
		const FStructuralNodeId PanelId = Session->MakeNodeId(Panel);
		FTrackedEndpoint& Tracked = Session->TrackedEndpoints().FindOrAdd(PanelId);
		if (!FStructuralPowerModConfig::IsGroupLightingEnabled()
			|| PanelNeedsLightingReconcileRetry(Panel))
		{
			Tracked.bPanelLinksReady = false;
			Tracked.bDownstreamLinksReady = false;
		}
		FStructuralEndpointDispatcher::DispatchPlacement(
			*Session,
			Panel,
			/*bLocalPromoteOnly=*/false,
			/*bOverrideAttachContext=*/true,
			EAttachContext::RuntimePlace);
		if (FStructuralPowerModConfig::IsGroupLightingEnabled())
		{
			LogPanelReconcileSummary(Panel);
		}
		else
		{
			FStructuralPanelControlledSync::ReleaseIntegratedSubnet(*Session, Panel);
		}
	}

	if (FStructuralPowerModConfig::IsGroupLightingEnabled())
	{
		UE_LOG(LogStructuralPower, Log,
			TEXT("[HALSP] Post-load panel reconcile complete — %d panel(s)"),
			Panels.Num());

		ApplyKeyedSubnetAllPanels();
	}
	else
	{
		UE_LOG(LogStructuralPower, Log,
			TEXT("[HALSP] GroupLighting off — tore down structural links on %d panel(s)"),
			Panels.Num());
	}
}

void FStructuralPowerReconcile::ApplyKeyedSubnetAllPanels()
{
	if (!FStructuralPowerModConfig::IsGroupLightingEnabled())
	{
		return;
	}

	TArray<AFGBuildableLightsControlPanel*> Panels;
	CollectKnownPanelEndpoints(Panels);

	for (AFGBuildableLightsControlPanel* Panel : Panels)
	{
		FStructuralPanelControlledSync::ApplyKeyedSubnet(*Session, Panel);
	}
}

void FStructuralPowerReconcile::RefreshKeyedTransferAfterLoad()
{
	if (!FStructuralPowerModConfig::IsGroupLightingEnabled())
	{
		return;
	}

	Session->RefreshBridgeEndpointRootIndex();

	TSet<int32> Roots;
	for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : Session->TrackedEndpoints())
	{
		if (Pair.Value.Kind != EStructuralEndpointKind::Switch)
		{
			continue;
		}

		AFGBuildableCircuitSwitch* Switch = Pair.Value.GetSwitch();
		if (!IsValid(Switch) || !Switch->IsBridgeActive())
		{
			continue;
		}

		const FName Control = Session->Ids().ResolveControl(Switch, EStructuralChannel::Switch);
		if (!FStructuralPowerRouter::IsAssignedControl(Control))
		{
			continue;
		}

		const int32 Root = Session->StructureGraph().FindRoot(Pair.Value.MountParentId);
		if (Root == INDEX_NONE)
		{
			continue;
		}

		Roots.Add(Root);

		if (const TArray<FStructuralNodeId>* PanelIds =
				Session->EndpointIndex().Get(Root, EStructuralEndpointKind::Panel))
		{
			for (const FStructuralNodeId& PanelId : *PanelIds)
			{
				if (FTrackedEndpoint* PanelTracked = Session->TrackedEndpoints().Find(PanelId))
				{
					if (Session->Ids().ResolveSource(
							PanelTracked->GetPanel(),
							EStructuralChannel::Light) != Control)
					{
						continue;
					}

					PanelTracked->bPanelLinksReady = false;
					PanelTracked->bDownstreamLinksReady = false;
				}
			}
		}
	}

	for (int32 Root : Roots)
	{
		FStructuralPowerContext Ctx =
			Session->MakeProcessorContext(EAttachContext::RuntimePlace, Root);
		FStructuralPowerTransferGate::RefreshKeyedTransferOnRoot(
			Ctx,
			Root,
			/*bLocalPromoteOnly=*/false);
	}
}

void FStructuralPowerReconcile::EnumerateTrackedLightsOnRoot(
	int32 Root,
	TFunctionRef<void(AFGBuildableLightSource*)> Visitor)
{
	if (Root == INDEX_NONE)
	{
		return;
	}

	Session->BridgeRootIndex().RefreshBridgeEndpointRootIndex();

	const TArray<FStructuralNodeId>* LightIds =
		Session->EndpointIndex().Get(Root, EStructuralEndpointKind::Light);
	if (!LightIds)
	{
		return;
	}

	for (const FStructuralNodeId& LightId : *LightIds)
	{
		const FTrackedEndpoint* Tracked = Session->TrackedEndpoints().Find(LightId);
		if (!Tracked)
		{
			continue;
		}

		if (AFGBuildableLightSource* Light = Tracked->GetLight())
		{
			Visitor(Light);
		}
	}
}

void FStructuralPowerReconcile::RefreshNamedControlPanelsAfterLoad()
{
	if (!FStructuralPowerModConfig::IsGroupLightingEnabled())
	{
		return;
	}

	Session->RefreshBridgeEndpointRootIndex();

	TArray<AFGBuildableLightsControlPanel*> Panels;
	CollectKnownPanelEndpoints(Panels);

	for (AFGBuildableLightsControlPanel* Panel : Panels)
	{
		if (!IsValid(Panel))
		{
			continue;
		}

		const FName Control = Session->Ids().ResolveControl(Panel, EStructuralChannel::Light);
		if (Control.IsNone() || Control == StructuralPowerConstants::ControlUnconfigured)
		{
			continue;
		}

		const FName Source = Session->Ids().ResolveSource(Panel, EStructuralChannel::Light);
		const FStructuralWallAnchor Anchor = Session->ResolveOutletAnchor(Panel);
		FStructuralNodeId ParentId;
		const int32 Root =
			Session->BridgeRootIndex().ResolveEndpointComponentRoot(Panel, Anchor, ParentId);
		if (Root != INDEX_NONE && !Source.IsNone())
		{
			if (const TArray<FStructuralNodeId>* SwitchIds =
					Session->EndpointIndex().Get(Root, EStructuralEndpointKind::Switch))
			{
				bool bSwitchKeyed = false;
				for (const FStructuralNodeId& SwitchId : *SwitchIds)
				{
					const FTrackedEndpoint* SwitchTracked =
						Session->TrackedEndpoints().Find(SwitchId);
					if (!SwitchTracked)
					{
						continue;
					}

					if (AFGBuildableCircuitSwitch* Switch = SwitchTracked->GetSwitch())
					{
						if (Session->Ids().ResolveControl(Switch, EStructuralChannel::Switch) == Source)
						{
							bSwitchKeyed = true;
							break;
						}
					}
				}

				if (bSwitchKeyed)
				{
					continue;
				}
			}
		}

		const FStructuralNodeId PanelId = Session->MakeNodeId(Panel);
		FTrackedEndpoint& Tracked = Session->TrackedEndpoints().FindOrAdd(PanelId);
		Tracked.bPanelLinksReady = false;
		Tracked.bDownstreamLinksReady = false;
		FStructuralEndpointDispatcher::DispatchPlacement(
			*Session,
			Panel,
			/*bLocalPromoteOnly=*/false,
			/*bOverrideAttachContext=*/true,
			EAttachContext::RuntimePlace);
		FStructuralPanelControlledSync::ApplyKeyedSubnet(*Session, Panel);
	}
}

void FStructuralPowerReconcile::SuspendAllIntegratedLighting()
{
	UWorld* World = Session->GetWorld();
	if (!IsValid(World) || IsEngineExitRequested())
	{
		return;
	}

	FStructuralPowerContext Ctx = Session->MakeProcessorContext(EAttachContext::Toggle);
	FStructuralPowerTransferGate::SuspendAllKeyedLightingTransfer(Ctx);

	TArray<AFGBuildableLightsControlPanel*> Panels;
	CollectKnownPanelEndpoints(Panels);
	for (AFGBuildableLightsControlPanel* Panel : Panels)
	{
		if (!IsValid(Panel))
		{
			continue;
		}

		const FStructuralNodeId PanelId = Session->MakeNodeId(Panel);
		FTrackedEndpoint& Tracked = Session->TrackedEndpoints().FindOrAdd(PanelId);
		Tracked.bPanelLinksReady = false;
		Tracked.bDownstreamLinksReady = false;
		Tracked.bStructuralPowerTransferActive = false;

		FStructuralPowerPanelProcessor::TearDown(Ctx, Panel);
		FStructuralPanelControlledSync::ReleaseIntegratedSubnet(*Session, Panel);
	}

	TArray<AFGBuildableLightSource*> Lights;
	TSet<AFGBuildableLightSource*> Seen;
	auto ConsiderLight = [&](AFGBuildableLightSource* Light)
	{
		if (!IsValid(Light) || Seen.Contains(Light))
		{
			return;
		}

		Seen.Add(Light);
		Lights.Add(Light);
	};

	for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : Session->TrackedEndpoints())
	{
		if (Pair.Value.Kind == EStructuralEndpointKind::Light)
		{
			ConsiderLight(Pair.Value.GetLight());
		}
	}

	if (Lights.Num() == 0)
	{
		if (AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(World))
		{
			for (AFGBuildable* Buildable : BuildableSubsystem->GetAllBuildablesRef())
			{
				if (FStructuralEligibilityRules::IsStructuralLightConsumer(Buildable))
				{
					ConsiderLight(FStructuralPowerBuildableCasts::AsLight(Buildable));
				}
			}
		}
	}

	for (AFGBuildableLightSource* Light : Lights)
	{
		const FStructuralNodeId LightId = Session->MakeNodeId(Light);
		FTrackedEndpoint& Tracked = Session->TrackedEndpoints().FindOrAdd(LightId);
		Tracked.bStructuralPowerTransferActive = false;
		FStructuralPowerLightProcessor::TearDown(Ctx, Light);
	}

	UE_LOG(LogStructuralPower, Log,
		TEXT("[HALSP] suspended integrated lighting — %d panel(s) %d light(s)"),
		Panels.Num(),
		Lights.Num());
}

void FStructuralPowerReconcile::ReconcileGroupLightingState()
{
	const bool bOn = FStructuralPowerModConfig::IsGroupLightingEnabled();

	UE_LOG(LogStructuralPower, Log,
		TEXT("[HALSP] GroupLighting reconcile — enabled=%d"),
		bOn ? 1 : 0);

	if (bOn)
	{
		if (!FStructuralPowerModConfig::CanMutateLiveConfig(Session->GetWorld()))
		{
			return;
		}

		ReconcileAllPanelEndpoints();
		RefreshKeyedTransferAfterLoad();
		RefreshNamedControlPanelsAfterLoad();
		ReconcileAllLightConsumers();
		Session->FinalLightingReconcilePass() = 0;
		Session->ScheduleFinalLightingReconcile();
	}
	else
	{
		Session->PendingFinalLightingReconcile() = false;
		Session->FinalLightingReconcilePass() = 0;

		if (!FStructuralPowerModConfig::CanMutateLiveConfig(Session->GetWorld()))
		{
			return;
		}

		SuspendAllIntegratedLighting();

		ReconcileAllPanelEndpoints();
		ReconcileAllLightConsumers();
	}
}

void FStructuralPowerReconcile::ReconcileAllLightConsumers()
{
	UWorld* World = Session->GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	TArray<AFGBuildableLightSource*> Lights;
	TSet<AFGBuildableLightSource*> Seen;

	auto Consider = [&](AFGBuildableLightSource* Light)
	{
		if (!IsValid(Light) || Seen.Contains(Light))
		{
			return;
		}

		Seen.Add(Light);
		Lights.Add(Light);
	};

	for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : Session->TrackedEndpoints())
	{
		if (Pair.Value.Kind == EStructuralEndpointKind::Light)
		{
			Consider(Pair.Value.GetLight());
		}
	}

	if (Lights.Num() == 0)
	{
		if (AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(World))
		{
			for (AFGBuildable* Buildable : BuildableSubsystem->GetAllBuildablesRef())
			{
				if (FStructuralEligibilityRules::IsStructuralLightConsumer(Buildable))
				{
					Consider(FStructuralPowerBuildableCasts::AsLight(Buildable));
				}
			}
		}
	}

	UE_LOG(LogStructuralPower, Log,
		TEXT("Reconcile lights — GroupLighting=%d candidate(s)=%d"),
		FStructuralPowerModConfig::IsGroupLightingEnabled() ? 1 : 0,
		Lights.Num());

	for (AFGBuildableLightSource* Light : Lights)
	{
		FStructuralEndpointDispatcher::DispatchPlacement(*Session, Light);
	}

	Session->BridgeRootIndex().RefreshBridgeEndpointRootIndex();
}

void FStructuralPowerReconcile::RefreshPanelsForLightSourceOnRoot(
	int32 Root,
	FName LightSource)
{
	if (Root == INDEX_NONE || LightSource.IsNone()
		|| !FStructuralPowerModConfig::IsGroupLightingEnabled())
	{
		return;
	}

	Session->RefreshBridgeEndpointRootIndex();

	auto RefreshPanel = [&](AFGBuildableLightsControlPanel* Panel)
	{
		if (!IsValid(Panel))
		{
			return;
		}

		if (FStructuralPanelControlledSync::ResolveEffectiveLightControl(*Session, Panel)
			!= LightSource)
		{
			return;
		}

		const FStructuralNodeId PanelId = Session->MakeNodeId(Panel);
		FTrackedEndpoint& Tracked = Session->TrackedEndpoints().FindOrAdd(PanelId);
		Tracked.bPanelLinksReady = false;
		Tracked.bDownstreamLinksReady = false;
		FStructuralEndpointDispatcher::DispatchPlacement(*Session, Panel);
		FStructuralPanelControlledSync::ApplyKeyedSubnet(*Session, Panel);
	};

	if (const TArray<FStructuralNodeId>* PanelIds =
			Session->EndpointIndex().Get(Root, EStructuralEndpointKind::Panel))
	{
		for (const FStructuralNodeId& PanelId : *PanelIds)
		{
			if (const FTrackedEndpoint* Tracked = Session->TrackedEndpoints().Find(PanelId))
			{
				RefreshPanel(Tracked->GetPanel());
			}
		}
	}

	for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : Session->TrackedEndpoints())
	{
		if (Pair.Value.Kind != EStructuralEndpointKind::Panel)
		{
			continue;
		}

		const FStructuralWallAnchor Anchor = Session->ResolveOutletAnchor(Pair.Value.GetPanel());
		FStructuralNodeId ParentId;
		if (Session->BridgeRootIndex().ResolveEndpointComponentRoot(
				Pair.Value.GetPanel(),
				Anchor,
				ParentId) != Root)
		{
			continue;
		}

		RefreshPanel(Pair.Value.GetPanel());
	}
}
