// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Graph/FStructuralGraphCircuitEcho.h"

#include "Core/FStructuralGraphSession.h"
#include "Graph/FStructuralBridgeRootIndex.h"
#include "Reconcile/FStructuralPowerReconcile.h"
#include "Save/FStructuralGraphIdOps.h"

#include "Attach/FStructuralPanelAttach.h"
#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Buildables/FGBuildableLightsControlPanel.h"
#include "Circuit/FStructuralCircuitPromotionUtil.h"
#include "Config/FStructuralPowerModConfig.h"
#include "FGCircuitConnectionComponent.h"
#include "FGPowerConnectionComponent.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Graph/FStructuralSwitchParentResolver.h"
#include "Network/UStructuralPowerPanelListener.h"
#include "Panel/FStructuralPanelPortResolver.h"
#include "Routing/EStructuralChannel.h"
#include "Save/AStructuralPowerGraphSubsystem.h"

void FStructuralGraphCircuitEcho::Bind(FStructuralGraphSession* InSession)
{
	Session = InSession;
}

bool FStructuralGraphCircuitEcho::ShouldSkipPanelCircuitEcho(
	AFGBuildableLightsControlPanel* Panel,
	const TCHAR** OutReason)
{
	auto SetReason = [OutReason](const TCHAR* Reason)
	{
		if (OutReason)
		{
			*OutReason = Reason;
		}
	};

	if (!Session || !IsValid(Panel) || !FStructuralPowerModConfig::IsGroupLightingEnabled())
	{
		return false;
	}

	const FStructuralChannelKey ChannelKey = Session->Ids().ResolveChannelKeyForBuildable(Panel);
	const FStructuralWallAnchor ParentAnchor = Session->BridgeRootIndex().ResolveOutletAnchor(Panel);
	FStructuralNodeId ParentId;
	const int32 Root =
		Session->BridgeRootIndex().ResolveEndpointComponentRoot(Panel, ParentAnchor, ParentId);

	if (!ChannelKey.Source.IsNone()
		&& Root != INDEX_NONE
		&& Session->Reconcile().IsSwitchFeedOpen(Root, ChannelKey.Source))
	{
		SetReason(TEXT("skip_feed_open"));
		return true;
	}

	const FStructuralNodeId PanelId = Session->MakeNodeId(Panel);

	if (Session->SiteState().IsEchoScopeActive())
	{
		if (!Session->SiteState().IsEchoDirtySite(Root))
		{
			SetReason(TEXT("skip_echo_scope"));
			return true;
		}

		if (Session->SiteState().WasPanelToggleHandledInScope(PanelId))
		{
			SetReason(TEXT("skip_echo_toggle"));
			return true;
		}

		if (Session->SiteState().WasPanelEchoProcessedInScope(PanelId))
		{
			SetReason(TEXT("skip_echo_coalesce"));
			return true;
		}
	}

	const FTrackedEndpoint* Tracked = Session->TrackedEndpoints().Find(PanelId);
	if (!Tracked)
	{
		return false;
	}

	const bool bRoutingUnchanged = Tracked->bPanelLinksReady
		&& Tracked->CachedPanelRoot == Root
		&& Tracked->CachedPanelKey == ChannelKey;
	if (bRoutingUnchanged)
	{
		FStructuralPanelPorts Ports;
		if (FStructuralPanelPortResolver::Resolve(Panel, Ports))
		{
			UFGPowerConnectionComponent* InputPower =
				FStructuralPanelPortResolver::AsPowerConnection(Ports.Input);
			if (FStructuralCircuitPromotionUtil::ConnectorSuppliesPower(InputPower)
				|| FStructuralPanelAttach::SupplyAlreadyLinked(
					*Session,
					Panel,
					Ports,
					Root,
					ChannelKey))
			{
				SetReason(TEXT("skip_routing_unchanged"));
				return true;
			}
		}
	}

	return false;
}

bool FStructuralGraphCircuitEcho::ShouldSkipSwitchCircuitEcho(
	AFGBuildableCircuitSwitch* Switch,
	const TCHAR** OutReason)
{
	auto SetReason = [OutReason](const TCHAR* Reason)
	{
		if (OutReason)
		{
			*OutReason = Reason;
		}
	};

	if (!Session || !IsValid(Switch))
	{
		return false;
	}

	const FStructuralNodeId SwitchId = Session->MakeNodeId(Switch);
	const FTrackedEndpoint* Tracked = Session->TrackedEndpoints().Find(SwitchId);
	FStructuralNodeId ParentId;
	int32 Root = INDEX_NONE;
	if (Tracked && Tracked->MountParentId.IsValid())
	{
		ParentId = Tracked->MountParentId;
		Root = Session->StructureGraph().FindRoot(ParentId);
	}
	else
	{
		const FStructuralWallAnchor ParentAnchor = Session->BridgeRootIndex().ResolveOutletAnchor(Switch);
		Root = Session->BridgeRootIndex().ResolveEndpointComponentRoot(Switch, ParentAnchor, ParentId);
	}

	if (Session->SiteState().IsEchoScopeActive())
	{
		if (!Session->SiteState().IsEchoDirtySite(Root))
		{
			SetReason(TEXT("skip_echo_scope"));
			return true;
		}

		if (Session->SiteState().WasSwitchToggleHandledInScope(SwitchId))
		{
			SetReason(TEXT("skip_echo_toggle"));
			return true;
		}

		if (Session->SiteState().WasSwitchEchoProcessedInScope(SwitchId))
		{
			SetReason(TEXT("skip_echo_coalesce"));
			return true;
		}
	}

	return false;
}

void FStructuralGraphCircuitEcho::MarkEchoDirtyForSwitchToggle(
	AFGBuildableCircuitSwitch* Switch,
	const int32 LocalRoot)
{
	if (!Session)
	{
		return;
	}

	TArray<int32> DirtySites;
	if (LocalRoot != INDEX_NONE)
	{
		DirtySites.Add(LocalRoot);
	}

	if (FStructuralPowerModConfig::IsGroupLightingEnabled()
		&& IsValid(Switch)
		&& FStructuralSwitchParentResolver::IsWiredToStructureSide(Switch))
	{
		Session->CrossSite().RefreshCouplingsFromWiredSwitch(*Session, Switch, LocalRoot);

		TArray<int32> FeedAffectedSites;
		Session->CrossSite().TraceFeedAffected(*Session, Switch, LocalRoot, FeedAffectedSites);
		for (int32 Site : FeedAffectedSites)
		{
			DirtySites.AddUnique(Site);
		}
	}

	if (DirtySites.Num() == 0)
	{
		return;
	}

	Session->SiteState().BeginEchoScope(DirtySites);
}

void FStructuralGraphCircuitEcho::NotePanelCircuitEchoProcessed(AFGBuildableLightsControlPanel* Panel)
{
	if (!Session || !IsValid(Panel))
	{
		return;
	}

	Session->SiteState().NotePanelEchoProcessed(Session->MakeNodeId(Panel));
}

void FStructuralGraphCircuitEcho::NotePanelToggleHandled(AFGBuildableLightsControlPanel* Panel)
{
	if (!Session || !IsValid(Panel))
	{
		return;
	}

	Session->SiteState().NotePanelToggleHandled(Session->MakeNodeId(Panel));
}

void FStructuralGraphCircuitEcho::NoteSwitchCircuitEchoProcessed(AFGBuildableCircuitSwitch* Switch)
{
	if (!Session || !IsValid(Switch))
	{
		return;
	}

	Session->SiteState().NoteSwitchEchoProcessed(Session->MakeNodeId(Switch));
}

void FStructuralGraphCircuitEcho::NoteSwitchToggleHandled(AFGBuildableCircuitSwitch* Switch)
{
	if (!Session || !IsValid(Switch))
	{
		return;
	}

	Session->SiteState().NoteSwitchToggleHandled(Session->MakeNodeId(Switch));
}

void FStructuralGraphCircuitEcho::EnsurePanelListener(AFGBuildableLightsControlPanel* Panel)
{
	if (!Session || !IsValid(Panel))
	{
		return;
	}

	TInlineComponentArray<UStructuralPowerPanelListener*> Listeners;
	Panel->GetComponents(Listeners);
	if (Listeners.Num() > 0)
	{
		return;
	}

	UStructuralPowerPanelListener* Listener =
		NewObject<UStructuralPowerPanelListener>(Panel, NAME_None, RF_Transient);
	if (!Listener)
	{
		return;
	}

	Panel->AddInstanceComponent(Listener);
	Listener->RegisterComponent();
	Listener->BindSubsystem(&Session->Owner(), Panel);
}
