// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Panel/FStructuralPanelControlledSync.h"

#include "Buildables/FGBuildableControlPanelHost.h"
#include "Buildables/FGBuildableLightSource.h"
#include "Buildables/FGBuildableLightsControlPanel.h"
#include "Config/FStructuralPowerModConfig.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Routing/EStructuralChannel.h"
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
