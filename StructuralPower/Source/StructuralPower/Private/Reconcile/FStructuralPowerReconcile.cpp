// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Reconcile/FStructuralPowerReconcile.h"

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
#include "Processors/FStructuralPowerSwitchProcessor.h"
#include "Processors/FStructuralPowerTransferGate.h"
#include "Processors/FStructuralPowerPanelProcessor.h"
#include "Processors/FStructuralPowerLightProcessor.h"
#include "Routing/FStructuralPowerRouter.h"
#include "Core/EAttachContext.h"
#include "Core/FStructuralPowerContext.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Graph/FStructuralPowerBuildableCasts.h"
#include "Rules/FStructuralEligibilityRules.h"
#include "StructuralPowerConstants.h"
#include "StructuralPowerLog.h"

void FStructuralPowerReconcile::Bind(AStructuralPowerGraphSubsystem* InSubsystem)
{
	Subsystem = InSubsystem;
}

void FStructuralPowerReconcile::MaybeRunPostLoadLightReconcile()
{
	if (Subsystem->GetPendingJobCount() > 0)
	{
		return;
	}

	if (Subsystem->bBulkLoadDrainActive)
	{
		Subsystem->FinishBulkLoadDrain();
	}

	if (!Subsystem->bPendingPostLoadLightReconcile)
	{
		Subsystem->bBulkLoadDrainActive = false;
		return;
	}

	Subsystem->bBulkLoadDrainActive = false;

	if (FStructuralPowerModConfig::IsGroupLightingEnabled())
	{
		ReconcileAllPanelEndpoints();
		RefreshKeyedTransferAfterLoad();
		RefreshNamedControlPanelsAfterLoad();
		Subsystem->FinalLightingReconcilePass = 0;
		// FG circuits often settle one tick later — defer consumer attach.
		Subsystem->ScheduleFinalLightingReconcile();
	}

	Subsystem->bPendingPostLoadLightReconcile = false;
}

void FStructuralPowerReconcile::MaybeRunFinalLightingReconcile()
{
	if (!Subsystem->bPendingFinalLightingReconcile)
	{
		return;
	}

	if (Subsystem->GetPendingJobCount() > 0)
	{
		Subsystem->ScheduleFinalLightingReconcile();
		return;
	}

	if (!FStructuralPowerModConfig::IsGroupLightingEnabled())
	{
		Subsystem->bPendingFinalLightingReconcile = false;
		Subsystem->FinalLightingReconcilePass = 0;
		return;
	}

	++Subsystem->FinalLightingReconcilePass;

	UE_LOG(LogStructuralPower, Log,
		TEXT("[HALSP] Final lighting reconcile pass=%d/%d — panels + keyed transfer + lights"),
		Subsystem->FinalLightingReconcilePass,
		AStructuralPowerGraphSubsystem::MaxFinalLightingReconcilePasses);

	ReconcileAllPanelEndpoints();
	RefreshKeyedTransferAfterLoad();
	ReconcileAllLightConsumers();

	if (LightingReconcileNeedsRetry()
		&& Subsystem->FinalLightingReconcilePass
			< AStructuralPowerGraphSubsystem::MaxFinalLightingReconcilePasses)
	{
		Subsystem->ScheduleFinalLightingReconcile();
		return;
	}

	Subsystem->FinalLightingReconcilePass = 0;
	Subsystem->bPendingFinalLightingReconcile = false;
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
		if (!IsValid(Panel))
		{
			continue;
		}

		const FName EffectiveControl =
			FStructuralPanelControlledSync::ResolveEffectiveLightControl(*Subsystem, Panel);
		if (EffectiveControl.IsNone())
		{
			continue;
		}

		FStructuralPanelPorts Ports;
		if (!FStructuralPanelPortResolver::Resolve(Panel, Ports))
		{
			return true;
		}

		const FStructuralChannelKey ChannelKey =
			Subsystem->IdOps.ResolveChannelKeyForBuildable(Panel);
		const int32 Root = Subsystem->GetEndpointComponentRoot(Panel);
		if (Root == INDEX_NONE)
		{
			return true;
		}

		if (!FStructuralPanelAttach::SupplyAlreadyLinked(
				*Subsystem, Panel, Ports, Root, ChannelKey))
		{
			return true;
		}

		const UFGPowerConnectionComponent* InputPower =
			FStructuralPanelPortResolver::AsPowerConnection(Ports.Input);
		if (!FStructuralCircuitPromotionUtil::ConnectorSuppliesPower(InputPower))
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

	Subsystem->BridgeRootIndex.RefreshBridgeEndpointRootIndex();

	if (const TArray<FStructuralNodeId>* SwitchIds =
			Subsystem->EndpointIndex.Get(Root, EStructuralEndpointKind::Switch))
	{
		const TArray<FStructuralNodeId> SwitchIdsSnapshot = *SwitchIds;
		for (const FStructuralNodeId& NodeId : SwitchIdsSnapshot)
		{
			if (const FTrackedEndpoint* Tracked = Subsystem->TrackedEndpoints.Find(NodeId))
			{
				if (AFGBuildableCircuitSwitch* Switch = Tracked->GetSwitch())
				{
					if (Subsystem->IdOps.ResolveControl(Switch, EStructuralChannel::Switch) == LightKey.Source)
					{
						return true;
					}
				}
			}
		}
	}

	for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : Subsystem->TrackedEndpoints)
	{
		if (Pair.Value.Kind != EStructuralEndpointKind::Switch)
		{
			continue;
		}

		if (Subsystem->StructureGraph.FindRoot(Pair.Value.ParentId) != Root)
		{
			continue;
		}

		if (AFGBuildableCircuitSwitch* Switch = Pair.Value.GetSwitch())
		{
			if (Subsystem->IdOps.ResolveControl(Switch, EStructuralChannel::Switch) == LightKey.Source)
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

		const FStructuralWallAnchor Anchor = Subsystem->ResolveOutletAnchor(Panel);
		FStructuralNodeId ParentId;
		if (Subsystem->BridgeRootIndex.ResolveEndpointComponentRoot(Panel, Anchor, ParentId) != Root)
		{
			return false;
		}

		const FName PanelControl = Subsystem->IdOps.ResolveControl(Panel, EStructuralChannel::Light);
		return !PanelControl.IsNone()
			&& PanelControl != StructuralPowerConstants::ControlUnconfigured
			&& PanelControl == LightKey.Source;
	};

	Subsystem->BridgeRootIndex.RefreshBridgeEndpointRootIndex();

	if (const TArray<FStructuralNodeId>* PanelIds =
			Subsystem->EndpointIndex.Get(Root, EStructuralEndpointKind::Panel))
	{
		const TArray<FStructuralNodeId> PanelIdsSnapshot = *PanelIds;
		for (const FStructuralNodeId& PanelId : PanelIdsSnapshot)
		{
			if (const FTrackedEndpoint* Tracked = Subsystem->TrackedEndpoints.Find(PanelId))
			{
				if (PanelHostsLightGroup(Tracked->GetPanel()))
				{
					return true;
				}
			}
		}
	}

	for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : Subsystem->TrackedEndpoints)
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
	Subsystem->PlacementQueue.ForEachPendingOutletBuildable([&](AFGBuildable* Buildable)
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

	if (UWorld* World = Subsystem->GetWorld())
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

	if (const TArray<FStructuralNodeId>* SwitchIds =
			Subsystem->EndpointIndex.Get(Root, EStructuralEndpointKind::Switch))
	{
		for (const FStructuralNodeId& NodeId : *SwitchIds)
		{
			if (const FTrackedEndpoint* Tracked = Subsystem->TrackedEndpoints.Find(NodeId))
			{
				if (AFGBuildableCircuitSwitch* Switch = Tracked->GetSwitch())
				{
					if (Subsystem->IdOps.ResolveControl(Switch, EStructuralChannel::Switch) != SwitchControlId)
					{
						continue;
					}

					return !FStructuralPowerSwitchProcessor::ShouldInjectStructuralPath(Switch) || !Switch->IsBridgeActive();
				}
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

	const FStructuralChannelKey ChannelKey = Subsystem->IdOps.ResolveChannelKeyForBuildable(Panel);
	const FStructuralWallAnchor ParentAnchor = Subsystem->ResolveOutletAnchor(Panel);
	FStructuralNodeId ParentId;
	const int32 Root = Subsystem->BridgeRootIndex.ResolveEndpointComponentRoot(Panel, ParentAnchor, ParentId);
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

	Subsystem->BridgeRootIndex.RefreshBridgeEndpointRootIndex();

	Subsystem->EndpointIndex.ForEachEndpoint(EStructuralEndpointKind::Panel, [&](const FStructuralNodeId& PanelId)
	{
		if (const FTrackedEndpoint* Tracked = Subsystem->TrackedEndpoints.Find(PanelId))
		{
			ConsiderPanel(Tracked->GetPanel());
		}
	});

	for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : Subsystem->TrackedEndpoints)
	{
		if (Pair.Value.Kind != EStructuralEndpointKind::Panel)
		{
			continue;
		}

		ConsiderPanel(Pair.Value.GetPanel());
	}

	Subsystem->PlacementQueue.ForEachPendingOutletBuildable([&](AFGBuildable* Buildable)
	{
		ConsiderPanel(FStructuralPowerBuildableCasts::AsPanel(Buildable));
	});

	if (Seen.Num() > 0)
	{
		return;
	}

	if (UWorld* World = Subsystem->GetWorld())
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
		const FStructuralNodeId PanelId = AStructuralPowerGraphSubsystem::MakeNodeId(Panel);
		FTrackedEndpoint& Tracked = Subsystem->TrackedEndpoints.FindOrAdd(PanelId);
		Tracked.bPanelLinksReady = false;
		Tracked.bDownstreamLinksReady = false;
		Subsystem->ProcessPanelEndpoint(Panel);
		if (FStructuralPowerModConfig::IsGroupLightingEnabled())
		{
			LogPanelReconcileSummary(Panel);
		}
		else
		{
			FStructuralPanelControlledSync::ReleaseIntegratedSubnet(*Subsystem, Panel);
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
		FStructuralPanelControlledSync::ApplyKeyedSubnet(*Subsystem, Panel);
	}
}

void FStructuralPowerReconcile::RefreshKeyedTransferAfterLoad()
{
	if (!FStructuralPowerModConfig::IsGroupLightingEnabled())
	{
		return;
	}

	Subsystem->RefreshBridgeEndpointRootIndex();

	TSet<int32> Roots;
	for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : Subsystem->TrackedEndpoints)
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

		const FName Control = Subsystem->IdOps.ResolveControl(Switch, EStructuralChannel::Switch);
		if (!FStructuralPowerRouter::IsAssignedControl(Control))
		{
			continue;
		}

		const int32 Root = Subsystem->StructureGraph.FindRoot(Pair.Value.ParentId);
		if (Root == INDEX_NONE)
		{
			continue;
		}

		Roots.Add(Root);

		if (const TArray<FStructuralNodeId>* PanelIds =
				Subsystem->EndpointIndex.Get(Root, EStructuralEndpointKind::Panel))
		{
			for (const FStructuralNodeId& PanelId : *PanelIds)
			{
				if (FTrackedEndpoint* PanelTracked = Subsystem->TrackedEndpoints.Find(PanelId))
				{
					if (Subsystem->IdOps.ResolveSource(
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
			Subsystem->MakeProcessorContext(EAttachContext::RuntimePlace, Root);
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

	Subsystem->BridgeRootIndex.RefreshBridgeEndpointRootIndex();

	const TArray<FStructuralNodeId>* LightIds =
		Subsystem->EndpointIndex.Get(Root, EStructuralEndpointKind::Light);
	if (!LightIds)
	{
		return;
	}

	for (const FStructuralNodeId& LightId : *LightIds)
	{
		const FTrackedEndpoint* Tracked = Subsystem->TrackedEndpoints.Find(LightId);
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

	Subsystem->RefreshBridgeEndpointRootIndex();

	TArray<AFGBuildableLightsControlPanel*> Panels;
	CollectKnownPanelEndpoints(Panels);

	for (AFGBuildableLightsControlPanel* Panel : Panels)
	{
		if (!IsValid(Panel))
		{
			continue;
		}

		const FName Control = Subsystem->IdOps.ResolveControl(Panel, EStructuralChannel::Light);
		if (Control.IsNone() || Control == StructuralPowerConstants::ControlUnconfigured)
		{
			continue;
		}

		const FName Source = Subsystem->IdOps.ResolveSource(Panel, EStructuralChannel::Light);
		const FStructuralWallAnchor Anchor = Subsystem->ResolveOutletAnchor(Panel);
		FStructuralNodeId ParentId;
		const int32 Root =
			Subsystem->BridgeRootIndex.ResolveEndpointComponentRoot(Panel, Anchor, ParentId);
		if (Root != INDEX_NONE && !Source.IsNone())
		{
			if (const TArray<FStructuralNodeId>* SwitchIds =
					Subsystem->EndpointIndex.Get(Root, EStructuralEndpointKind::Switch))
			{
				bool bSwitchKeyed = false;
				for (const FStructuralNodeId& SwitchId : *SwitchIds)
				{
					const FTrackedEndpoint* SwitchTracked =
						Subsystem->TrackedEndpoints.Find(SwitchId);
					if (!SwitchTracked)
					{
						continue;
					}

					if (AFGBuildableCircuitSwitch* Switch = SwitchTracked->GetSwitch())
					{
						if (Subsystem->IdOps.ResolveControl(Switch, EStructuralChannel::Switch) == Source)
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

		const FStructuralNodeId PanelId = AStructuralPowerGraphSubsystem::MakeNodeId(Panel);
		FTrackedEndpoint& Tracked = Subsystem->TrackedEndpoints.FindOrAdd(PanelId);
		Tracked.bPanelLinksReady = false;
		Tracked.bDownstreamLinksReady = false;
		Subsystem->ProcessPanelEndpoint(Panel, /*bLocalPromoteOnly=*/false);
		FStructuralPanelControlledSync::ApplyKeyedSubnet(*Subsystem, Panel);
	}
}

void FStructuralPowerReconcile::SuspendAllIntegratedLighting()
{
	UWorld* World = Subsystem->GetWorld();
	if (!IsValid(World) || IsEngineExitRequested())
	{
		return;
	}

	FStructuralPowerContext Ctx = Subsystem->MakeProcessorContext(EAttachContext::Toggle);
	FStructuralPowerTransferGate::SuspendAllKeyedLightingTransfer(Ctx);

	TArray<AFGBuildableLightsControlPanel*> Panels;
	CollectKnownPanelEndpoints(Panels);
	for (AFGBuildableLightsControlPanel* Panel : Panels)
	{
		if (!IsValid(Panel))
		{
			continue;
		}

		const FStructuralNodeId PanelId = AStructuralPowerGraphSubsystem::MakeNodeId(Panel);
		FTrackedEndpoint& Tracked = Subsystem->TrackedEndpoints.FindOrAdd(PanelId);
		Tracked.bPanelLinksReady = false;
		Tracked.bDownstreamLinksReady = false;
		Tracked.bStructuralPowerTransferActive = false;

		FStructuralPowerPanelProcessor::TearDown(Ctx, Panel);
		FStructuralPanelControlledSync::ReleaseIntegratedSubnet(*Subsystem, Panel);
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

	for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : Subsystem->TrackedEndpoints)
	{
		if (Pair.Value.Kind == EStructuralEndpointKind::Light)
		{
			ConsiderLight(Pair.Value.GetLight());
		}
	}

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

	for (AFGBuildableLightSource* Light : Lights)
	{
		const FStructuralNodeId LightId = AStructuralPowerGraphSubsystem::MakeNodeId(Light);
		FTrackedEndpoint& Tracked = Subsystem->TrackedEndpoints.FindOrAdd(LightId);
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
		if (!FStructuralPowerModConfig::CanMutateLiveConfig(Subsystem->GetWorld()))
		{
			return;
		}

		ReconcileAllPanelEndpoints();
		RefreshKeyedTransferAfterLoad();
		RefreshNamedControlPanelsAfterLoad();
		ReconcileAllLightConsumers();
		Subsystem->FinalLightingReconcilePass = 0;
		Subsystem->ScheduleFinalLightingReconcile();
	}
	else
	{
		Subsystem->bPendingFinalLightingReconcile = false;
		Subsystem->FinalLightingReconcilePass = 0;

		if (!FStructuralPowerModConfig::CanMutateLiveConfig(Subsystem->GetWorld()))
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
	UWorld* World = Subsystem->GetWorld();
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

	for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : Subsystem->TrackedEndpoints)
	{
		if (Pair.Value.Kind == EStructuralEndpointKind::Light)
		{
			Consider(Pair.Value.GetLight());
		}
	}

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

	UE_LOG(LogStructuralPower, Log,
		TEXT("Reconcile lights — GroupLighting=%d candidate(s)=%d"),
		FStructuralPowerModConfig::IsGroupLightingEnabled() ? 1 : 0,
		Lights.Num());

	for (AFGBuildableLightSource* Light : Lights)
	{
		Subsystem->ProcessLightEndpoint(Light);
	}

	Subsystem->BridgeRootIndex.RefreshBridgeEndpointRootIndex();
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

	Subsystem->RefreshBridgeEndpointRootIndex();

	auto RefreshPanel = [&](AFGBuildableLightsControlPanel* Panel)
	{
		if (!IsValid(Panel))
		{
			return;
		}

		if (FStructuralPanelControlledSync::ResolveEffectiveLightControl(*Subsystem, Panel)
			!= LightSource)
		{
			return;
		}

		const FStructuralNodeId PanelId = AStructuralPowerGraphSubsystem::MakeNodeId(Panel);
		FTrackedEndpoint& Tracked = Subsystem->TrackedEndpoints.FindOrAdd(PanelId);
		Tracked.bPanelLinksReady = false;
		Tracked.bDownstreamLinksReady = false;
		Subsystem->ProcessPanelEndpoint(Panel);
		FStructuralPanelControlledSync::ApplyKeyedSubnet(*Subsystem, Panel);
	};

	if (const TArray<FStructuralNodeId>* PanelIds =
			Subsystem->EndpointIndex.Get(Root, EStructuralEndpointKind::Panel))
	{
		for (const FStructuralNodeId& PanelId : *PanelIds)
		{
			if (const FTrackedEndpoint* Tracked = Subsystem->TrackedEndpoints.Find(PanelId))
			{
				RefreshPanel(Tracked->GetPanel());
			}
		}
	}

	for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : Subsystem->TrackedEndpoints)
	{
		if (Pair.Value.Kind != EStructuralEndpointKind::Panel)
		{
			continue;
		}

		const FStructuralWallAnchor Anchor = Subsystem->ResolveOutletAnchor(Pair.Value.GetPanel());
		FStructuralNodeId ParentId;
		if (Subsystem->BridgeRootIndex.ResolveEndpointComponentRoot(
				Pair.Value.GetPanel(),
				Anchor,
				ParentId) != Root)
		{
			continue;
		}

		RefreshPanel(Pair.Value.GetPanel());
	}
}
