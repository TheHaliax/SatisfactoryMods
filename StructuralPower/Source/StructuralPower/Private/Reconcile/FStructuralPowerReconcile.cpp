// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Reconcile/FStructuralPowerReconcile.h"

#include "Save/AStructuralPowerGraphSubsystem.h"

#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Buildables/FGBuildableLightSource.h"
#include "Buildables/FGBuildableLightsControlPanel.h"
#include "Circuit/FStructuralCircuitPromotionUtil.h"
#include "Config/FStructuralPowerModConfig.h"
#include "Diagnostics/FStructuralPowerTrace.h"
#include "FGBuildableSubsystem.h"
#include "FGPowerConnectionComponent.h"
#include "Panel/FStructuralPanelControlledSync.h"
#include "Panel/FStructuralPanelPortResolver.h"
#include "Processors/FStructuralPowerSwitchProcessor.h"
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
		return;
	}

	Subsystem->bPendingPostLoadLightReconcile = false;
	if (FStructuralPowerModConfig::IsGroupLightingEnabled())
	{
		// Panels first — downstream links must exist before light reconcile classifies
		ReconcileAllPanelEndpoints();
		ReconcileAllLightConsumers();
	}
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
				if (AFGBuildableCircuitSwitch* Switch =
						Cast<AFGBuildableCircuitSwitch>(Tracked->Actor.Get()))
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

		if (AFGBuildableCircuitSwitch* Switch =
				Cast<AFGBuildableCircuitSwitch>(Pair.Value.Actor.Get()))
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
				if (PanelHostsLightGroup(
						Cast<AFGBuildableLightsControlPanel>(Tracked->Actor.Get())))
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

		if (PanelHostsLightGroup(
				Cast<AFGBuildableLightsControlPanel>(Pair.Value.Actor.Get())))
		{
			return true;
		}
	}

	bool bFoundPendingPanel = false;
	Subsystem->PlacementQueue.ForEachPendingOutletBuildable([&](AFGBuildable* Buildable)
	{
		if (!bFoundPendingPanel
			&& PanelHostsLightGroup(Cast<AFGBuildableLightsControlPanel>(Buildable)))
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
				if (PanelHostsLightGroup(Cast<AFGBuildableLightsControlPanel>(Buildable)))
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
				if (AFGBuildableCircuitSwitch* Switch =
						Cast<AFGBuildableCircuitSwitch>(Tracked->Actor.Get()))
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
			ConsiderPanel(Cast<AFGBuildableLightsControlPanel>(Tracked->Actor.Get()));
		}
	});

	for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : Subsystem->TrackedEndpoints)
	{
		if (Pair.Value.Kind != EStructuralEndpointKind::Panel)
		{
			continue;
		}

		ConsiderPanel(Cast<AFGBuildableLightsControlPanel>(Pair.Value.Actor.Get()));
	}

	Subsystem->PlacementQueue.ForEachPendingOutletBuildable([&](AFGBuildable* Buildable)
	{
		ConsiderPanel(Cast<AFGBuildableLightsControlPanel>(Buildable));
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
				ConsiderPanel(Cast<AFGBuildableLightsControlPanel>(Buildable));
			}
		}
	}
}

void FStructuralPowerReconcile::ReconcileAllPanelEndpoints()
{
	if (!FStructuralPowerModConfig::IsGroupLightingEnabled())
	{
		return;
	}

	TArray<AFGBuildableLightsControlPanel*> Panels;
	CollectKnownPanelEndpoints(Panels);

	for (AFGBuildableLightsControlPanel* Panel : Panels)
	{
		const FStructuralNodeId PanelId = AStructuralPowerGraphSubsystem::MakeNodeId(Panel);
		FTrackedEndpoint& Tracked = Subsystem->TrackedEndpoints.FindOrAdd(PanelId);
		Tracked.bPanelLinksReady = false;
		Tracked.bDownstreamLinksReady = false;
		Subsystem->ProcessPanelEndpoint(Panel);
		LogPanelReconcileSummary(Panel);
	}

	UE_LOG(LogStructuralPower, Log,
		TEXT("[HALSP] Post-load panel reconcile complete — %d panel(s)"),
		Panels.Num());

	ApplyKeyedSubnetAllPanels();
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

		if (AFGBuildableLightSource* Light = Cast<AFGBuildableLightSource>(Tracked->Actor.Get()))
		{
			Visitor(Light);
		}
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
			Consider(Cast<AFGBuildableLightSource>(Pair.Value.Actor.Get()));
		}
	}

	if (AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(World))
	{
		for (AFGBuildable* Buildable : BuildableSubsystem->GetAllBuildablesRef())
		{
			if (FStructuralEligibilityRules::IsStructuralLightConsumer(Buildable))
			{
				Consider(Cast<AFGBuildableLightSource>(Buildable));
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
