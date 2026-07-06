// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Panel/FStructuralPanelControlledSync.h"

#include "Buildables/FGBuildableControlPanelHost.h"
#include "Buildables/FGBuildableLightSource.h"
#include "Buildables/FGBuildableLightsControlPanel.h"
#include "Config/FStructuralPowerModConfig.h"
#include "Routing/EStructuralChannel.h"
#include "Save/AStructuralPowerGraphSubsystem.h"
#include "StructuralPowerConstants.h"
#include "StructuralPowerLog.h"
#include "UObject/UnrealType.h"

namespace
{
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

	const FName Control = Graph.ResolveControl(Panel, EStructuralChannel::Light);
	if (Control.IsNone() || Control == StructuralPowerConstants::ControlUnconfigured)
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
		if (LightSource == Control)
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
		TEXT("[PWR] panel %s keyed controlled=%d control=%s"),
		*Panel->GetName(),
		KeyedLights.Num(),
		*Control.ToString());
}
