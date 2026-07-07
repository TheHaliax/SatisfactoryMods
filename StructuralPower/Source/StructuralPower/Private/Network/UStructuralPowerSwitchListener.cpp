// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Network/UStructuralPowerSwitchListener.h"

#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Config/FStructuralPowerModConfig.h"
#include "Save/AStructuralPowerGraphSubsystem.h"
#include "Session/FStructuralPowerSessionSettings.h"

void UStructuralPowerSwitchListener::BindSubsystem(
	AStructuralPowerGraphSubsystem* Graph,
	AFGBuildableCircuitSwitch* Switch)
{
	if (!IsValid(Graph) || !IsValid(Switch))
	{
		return;
	}

	GraphSubsystem = Graph;
	BoundSwitch = Switch;

	Switch->mOnIsSwitchOnChanged.AddDynamic(this, &UStructuralPowerSwitchListener::HandleSwitchOnChanged);
	Switch->mOnCircuitsChanged.AddDynamic(this, &UStructuralPowerSwitchListener::HandleCircuitsChanged);
}

void UStructuralPowerSwitchListener::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AFGBuildableCircuitSwitch* Switch = BoundSwitch.Get())
	{
		Switch->mOnIsSwitchOnChanged.RemoveDynamic(
			this,
			&UStructuralPowerSwitchListener::HandleSwitchOnChanged);
		Switch->mOnCircuitsChanged.RemoveDynamic(
			this,
			&UStructuralPowerSwitchListener::HandleCircuitsChanged);
	}

	Super::EndPlay(EndPlayReason);
}

void UStructuralPowerSwitchListener::HandleSwitchOnChanged()
{
	if (AStructuralPowerGraphSubsystem* Graph = GraphSubsystem.Get())
	{
		Graph->OnSwitchStateChanged(BoundSwitch.Get());
	}
}

void UStructuralPowerSwitchListener::HandleCircuitsChanged()
{
	if (AStructuralPowerGraphSubsystem* Graph = GraphSubsystem.Get())
	{
		if (Graph->ShouldDeferSwitchCircuitRefresh())
		{
			return;
		}

		if (Graph->IsBulkLoadDrainActive())
		{
			return;
		}

		// Manual groups: toggle owns restitch — circuit-changed must not re-enter switch attach.
		if (FStructuralPowerModConfig::IsPowerSwitchManualGroupsEnabled())
		{
			return;
		}

		if (AFGBuildableCircuitSwitch* Switch = BoundSwitch.Get())
		{
			Graph->ProcessSwitchWireDelta(Switch);
		}
	}
}
