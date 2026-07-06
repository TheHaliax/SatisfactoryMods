// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Panel/FStructuralPanelPortResolver.h"

#include "Buildables/FGBuildableCircuitBridge.h"
#include "Buildables/FGBuildableControlPanelHost.h"
#include "FGCircuitConnectionComponent.h"
#include "FGPowerConnectionComponent.h"
#include "UObject/UnrealType.h"

UFGCircuitConnectionComponent* ResolveDownstreamProperty(AFGBuildableControlPanelHost* Panel)
{
	if (!IsValid(Panel))
	{
		return nullptr;
	}

	const FObjectProperty* Prop = FindFProperty<FObjectProperty>(
		AFGBuildableControlPanelHost::StaticClass(),
		TEXT("mDownstreamConnection"));
	if (!Prop)
	{
		return nullptr;
	}

	return Cast<UFGCircuitConnectionComponent>(Prop->GetObjectPropertyValue_InContainer(Panel));
}

bool FStructuralPanelPortResolver::Resolve(
	AFGBuildableControlPanelHost* Panel,
	FStructuralPanelPorts& Out)
{
	Out = {};
	if (!IsValid(Panel))
	{
		return false;
	}

	Out.Downstream = ResolveDownstreamProperty(Panel);
	if (!IsValid(Out.Downstream))
	{
		return false;
	}

	UFGCircuitConnectionComponent* Conn0 = Panel->GetConnection0();
	UFGCircuitConnectionComponent* Conn1 = Panel->GetConnection1();
	if (Conn0 == Out.Downstream)
	{
		Out.Input = Conn1;
	}
	else if (Conn1 == Out.Downstream)
	{
		Out.Input = Conn0;
	}
	else
	{
		Out.Input = Conn0;
	}

	return IsValid(Out.Input);
}

UFGPowerConnectionComponent* FStructuralPanelPortResolver::AsPowerConnection(
	UFGCircuitConnectionComponent* Conn)
{
	return Cast<UFGPowerConnectionComponent>(Conn);
}
