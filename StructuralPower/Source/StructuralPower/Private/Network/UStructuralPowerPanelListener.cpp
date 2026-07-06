// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Network/UStructuralPowerPanelListener.h"

#include "Buildables/FGBuildableLightsControlPanel.h"
#include "Config/FStructuralPowerModConfig.h"
#include "Panel/FStructuralPanelControlledSync.h"
#include "Save/AStructuralPowerGraphSubsystem.h"

void UStructuralPowerPanelListener::BindSubsystem(
	AStructuralPowerGraphSubsystem* Graph,
	AFGBuildableLightsControlPanel* Panel)
{
	if (!IsValid(Graph) || !IsValid(Panel))
	{
		return;
	}

	GraphSubsystem = Graph;
	BoundPanel = Panel;

	Panel->mOnControlledBuildablesChanged.AddDynamic(
		this,
		&UStructuralPowerPanelListener::HandleControlledBuildablesChanged);
}

void UStructuralPowerPanelListener::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AFGBuildableLightsControlPanel* Panel = BoundPanel.Get())
	{
		Panel->mOnControlledBuildablesChanged.RemoveDynamic(
			this,
			&UStructuralPowerPanelListener::HandleControlledBuildablesChanged);
	}

	Super::EndPlay(EndPlayReason);
}

void UStructuralPowerPanelListener::HandleControlledBuildablesChanged()
{
	AStructuralPowerGraphSubsystem* Graph = GraphSubsystem.Get();
	AFGBuildableLightsControlPanel* Panel = BoundPanel.Get();
	if (!IsValid(Graph) || !IsValid(Panel))
	{
		return;
	}

	if (Graph->ShouldDeferSwitchCircuitRefresh())
	{
		return;
	}

	if (Graph->IsBuildablePlacementPending(Panel))
	{
		return;
	}

	if (FStructuralPowerModConfig::IsGroupLightingEnabled())
	{
		FStructuralPanelControlledSync::ApplyKeyedSubnet(*Graph, Panel);
		return;
	}

	Graph->EnqueuePlacement(Panel, EStructuralPlacementJobType::Outlet, /*bDefer=*/true);
}
