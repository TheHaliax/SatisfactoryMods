// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Panel/FStructuralPanelControlledSync.h"

#include "Components/UFGStructuralPowerConnectionComponent.h"
#include "Attach/FStructuralPanelAttach.h"
#include "Buildables/FGBuildableControlPanelHost.h"
#include "Buildables/FGBuildableLightSource.h"
#include "Buildables/FGBuildableLightsControlPanel.h"
#include "Config/FStructuralPowerModConfig.h"
#include "FGPowerConnectionComponent.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Misc/App.h"
#include "Panel/FStructuralPanelPortResolver.h"
#include "Routing/EStructuralChannel.h"
#include "Routing/FStructuralPowerRouter.h"
#include "Save/AStructuralPowerGraphSubsystem.h"
#include "StructuralPowerConstants.h"
#include "StructuralPowerLog.h"
#include "UObject/UnrealType.h"

namespace
{
bool GPanelPeerMirrorGuard = false;

FArrayProperty* GetControlledBuildablesProperty()
{
	return FindFProperty<FArrayProperty>(
		AFGBuildableControlPanelHost::StaticClass(),
		TEXT("mControlledBuildables"));
}

bool ControlledSetsMatch(
	const TArray<AFGBuildable*>& Desired,
	const TArray<AFGBuildable*>& Current)
{
	if (Desired.Num() != Current.Num())
	{
		return false;
	}

	TSet<AFGBuildable*> DesiredSet(Desired);
	for (AFGBuildable* Buildable : Current)
	{
		if (!DesiredSet.Contains(Buildable))
		{
			return false;
		}
	}

	return true;
}

void WriteControlledBuildables(
	AFGBuildableControlPanelHost* Host,
	const TArray<AFGBuildable*>& Buildables)
{
	FArrayProperty* Prop = GetControlledBuildablesProperty();
	FObjectProperty* Inner = Prop
		? CastField<FObjectProperty>(Prop->Inner)
		: nullptr;
	if (!Prop || !Inner || !IsValid(Host))
	{
		return;
	}

	FScriptArrayHelper Helper(Prop, Prop->ContainerPtrToValuePtr<void>(Host));
	Helper.EmptyValues();

	for (AFGBuildable* Buildable : Buildables)
	{
		if (!IsValid(Buildable))
		{
			continue;
		}

		const int32 Index = Helper.AddValue();
		Inner->SetObjectPropertyValue(Helper.GetRawPtr(Index), Buildable);
	}
}

bool PanelHasStructuralHiddenPath(
	AStructuralPowerGraphSubsystem& Graph,
	AFGBuildableLightsControlPanel* Panel,
	int32 Root,
	const FStructuralChannelKey& ChannelKey,
	const FStructuralPanelPorts& Ports)
{
	if (Root == INDEX_NONE || ChannelKey.Source.IsNone())
	{
		return false;
	}

	UFGPowerConnectionComponent* InputPower =
		FStructuralPanelPortResolver::AsPowerConnection(Ports.Input);
	if (!IsValid(InputPower))
	{
		return false;
	}

	UFGPowerConnectionComponent* Feed = Graph.ResolveSubnetFeedConnector(Root, ChannelKey);
	if (IsValid(Feed) && InputPower->HasHiddenConnection(Feed))
	{
		return true;
	}

	if (UFGPowerConnectionComponent* Downstream =
			FStructuralPanelPortResolver::AsPowerConnection(Ports.Downstream))
	{
		if (UFGStructuralPowerConnectionComponent* ControlBus =
				AStructuralPowerGraphSubsystem::FindPanelControlBus(Panel))
		{
			if (ControlBus->HasHiddenConnection(Downstream))
			{
				return true;
			}
		}
	}

	return false;
}

bool ShouldOverrideControlledBuildables(
	AStructuralPowerGraphSubsystem& Graph,
	AFGBuildableLightsControlPanel* Panel,
	int32 Root,
	const FStructuralChannelKey& ChannelKey,
	const FStructuralPanelPorts& Ports)
{
	if (!FStructuralPowerModConfig::IsGroupLightingEnabled() || Root == INDEX_NONE)
	{
		return false;
	}

	UFGPowerConnectionComponent* InputPower =
		FStructuralPanelPortResolver::AsPowerConnection(Ports.Input);
	if (!IsValid(InputPower))
	{
		return false;
	}

	if (PanelHasStructuralHiddenPath(Graph, Panel, Root, ChannelKey, Ports))
	{
		return true;
	}

	const FName Control = Graph.ResolveControl(Panel, EStructuralChannel::Light);
	return FStructuralPowerRouter::IsAssignedControl(Control);
}
} // namespace

FName FStructuralPanelControlledSync::ResolveEffectiveLightControl(
	AStructuralPowerGraphSubsystem& Graph,
	AFGBuildableLightsControlPanel* Panel)
{
	if (!IsValid(Panel))
	{
		return NAME_None;
	}

	const FName Control = Graph.ResolveControl(Panel, EStructuralChannel::Light);
	if (Control.IsNone() || Control == StructuralPowerConstants::ControlUnconfigured)
	{
		return Graph.ResolveSource(Panel, EStructuralChannel::Light);
	}

	return Control;
}

void FStructuralPanelControlledSync::ApplyKeyedSubnet(
	AStructuralPowerGraphSubsystem& Graph,
	AFGBuildableLightsControlPanel* Panel)
{
	if (!IsValid(Panel) || !Panel->HasAuthority())
	{
		return;
	}

	if (!FStructuralPowerModConfig::IsGroupLightingEnabled())
	{
		return;
	}

	const FName EffectiveControl = ResolveEffectiveLightControl(Graph, Panel);
	if (EffectiveControl.IsNone())
	{
		return;
	}

	const int32 Root = Graph.GetEndpointComponentRoot(Panel);
	if (Root == INDEX_NONE)
	{
		return;
	}

	FStructuralPanelPorts Ports;
	if (!FStructuralPanelPortResolver::Resolve(Panel, Ports))
	{
		return;
	}

	const FStructuralChannelKey ChannelKey = Graph.ResolveChannelKeyForBuildable(Panel);
	if (!ShouldOverrideControlledBuildables(Graph, Panel, Root, ChannelKey, Ports))
	{
		// FG AFGBuildableControlPanelHost::SearchDownstreamCircuit owns mControlledBuildables.
		return;
	}

	TArray<AFGBuildable*> KeyedLights;
	Graph.EnumerateTrackedLightsOnRoot(Root, [&](AFGBuildableLightSource* Light)
	{
		if (!IsValid(Light))
		{
			return;
		}

		const FName LightSource = Graph.ResolveSource(Light, EStructuralChannel::Light);
		if (LightSource == EffectiveControl)
		{
			KeyedLights.Add(Light);
		}
	});

	const TArray<AFGBuildable*> Current =
		Panel->GetControlledBuildables(AFGBuildableLightSource::StaticClass());
	if (ControlledSetsMatch(KeyedLights, Current))
	{
		return;
	}

	WriteControlledBuildables(Panel, KeyedLights);

	UE_LOG(LogStructuralPower, Verbose,
		TEXT("[HALSP] panel %s keyed controlled=%d control=%s"),
		*Panel->GetName(),
		KeyedLights.Num(),
		*EffectiveControl.ToString());
}

void FStructuralPanelControlledSync::ReleaseIntegratedSubnet(
	AStructuralPowerGraphSubsystem& Graph,
	AFGBuildableLightsControlPanel* Panel)
{
	if (!IsValid(Panel) || !Panel->HasAuthority() || IsEngineExitRequested())
	{
		return;
	}

	UWorld* World = Panel->GetWorld();
	if (!IsValid(World) || World->bIsTearingDown)
	{
		return;
	}

	const FName EffectiveControl = ResolveEffectiveLightControl(Graph, Panel);
	const FName PanelControl = Graph.ResolveControl(Panel, EStructuralChannel::Light);
	if (FStructuralPowerRouter::IsAssignedControl(EffectiveControl)
		|| FStructuralPowerRouter::IsAssignedControl(PanelControl))
	{
		WriteControlledBuildables(Panel, {});
	}

	if (UFunction* SearchFn = Panel->FindFunction(TEXT("SearchDownstreamCircuit")))
	{
		Panel->ProcessEvent(SearchFn, nullptr);
	}

	UE_LOG(LogStructuralPower, Log,
		TEXT("[HALSP] panel %s released integrated subnet — vanilla downstream search"),
		*Panel->GetName());
}

void FStructuralPanelControlledSync::MirrorSharedControlState(
	AStructuralPowerGraphSubsystem& Graph,
	AFGBuildableLightsControlPanel* Panel)
{
	if (GPanelPeerMirrorGuard || !IsValid(Panel) || !Panel->HasAuthority())
	{
		return;
	}

	if (!FStructuralPowerModConfig::IsGroupLightingEnabled())
	{
		return;
	}

	const FName Control = Graph.ResolveControl(Panel, EStructuralChannel::Light);
	const FName EffectiveControl = ResolveEffectiveLightControl(Graph, Panel);
	if (EffectiveControl.IsNone())
	{
		return;
	}

	const int32 Root = Graph.GetEndpointComponentRoot(Panel);
	if (Root == INDEX_NONE)
	{
		return;
	}

	Graph.RefreshBridgeEndpointRootIndex();
	const TArray<FStructuralNodeId>* PanelIds =
		Graph.EndpointIndex.Get(Root, EStructuralEndpointKind::Panel);
	if (!PanelIds || PanelIds->Num() == 0)
	{
		return;
	}

	const bool bEnabled = Panel->IsLightEnabled();
	const FLightSourceControlData ControlData = Panel->GetLightControlData();

	GPanelPeerMirrorGuard = true;
	for (const FStructuralNodeId& PeerId : *PanelIds)
	{
		const FTrackedEndpoint* Tracked = Graph.TrackedEndpoints.Find(PeerId);
		if (!Tracked)
		{
			continue;
		}

		AFGBuildableLightsControlPanel* Peer = Tracked->GetPanel();
		if (!IsValid(Peer) || Peer == Panel)
		{
			continue;
		}

		const FName PeerControl = Graph.ResolveControl(Peer, EStructuralChannel::Light);
		const bool bSameBank = Control.IsNone()
			|| Control == StructuralPowerConstants::ControlUnconfigured
			? (PeerControl.IsNone() || PeerControl == StructuralPowerConstants::ControlUnconfigured)
			: PeerControl == Control;
		if (!bSameBank)
		{
			continue;
		}

		if (Peer->IsLightEnabled() != bEnabled)
		{
			Peer->SetLightEnabled(bEnabled);
		}

		const FLightSourceControlData PeerData = Peer->GetLightControlData();
		if (PeerData.ColorSlotIndex != ControlData.ColorSlotIndex
			|| PeerData.Intensity != ControlData.Intensity
			|| PeerData.IsTimeOfDayAware != ControlData.IsTimeOfDayAware)
		{
			Peer->SetLightControlData(ControlData);
		}
	}
	GPanelPeerMirrorGuard = false;
}
