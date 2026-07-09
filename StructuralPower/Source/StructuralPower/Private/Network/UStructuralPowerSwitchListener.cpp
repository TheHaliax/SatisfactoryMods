// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Network/UStructuralPowerSwitchListener.h"

#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Core/FStructuralPowerContext.h"
#include "Engine/World.h"
#include "Graph/FStructuralSwitchParentResolver.h"
#include "Processors/FStructuralPowerSwitchProcessor.h"
#include "Save/AStructuralPowerGraphSubsystem.h"

namespace
{
static bool SwitchNeedsCircuitSubscription(
	AStructuralPowerGraphSubsystem& Graph,
	AFGBuildableCircuitSwitch* Switch)
{
	if (!IsValid(Switch))
	{
		return false;
	}

	const FStructuralPowerContext Ctx = Graph.MakeProcessorContext(EAttachContext::WireDelta);
	return FStructuralPowerSwitchProcessor::NeedsAdvancedWork(Ctx, Switch);
}
} // namespace

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
	SyncSubscriptions(Graph, Switch);
}

void UStructuralPowerSwitchListener::SyncSubscriptions(
	AStructuralPowerGraphSubsystem* Graph,
	AFGBuildableCircuitSwitch* Switch)
{
	if (!IsValid(Graph) || !IsValid(Switch))
	{
		return;
	}

	const bool bArmed = SwitchNeedsCircuitSubscription(*Graph, Switch);
	const bool bNeedsToggle = bArmed;
	const bool bNeedsCircuit = true;

	if (bCircuitsBound && !bNeedsCircuit)
	{
		Switch->mOnCircuitsChanged.RemoveDynamic(
			this,
			&UStructuralPowerSwitchListener::HandleCircuitsChanged);
		bCircuitsBound = false;
	}
	else if (!bCircuitsBound && bNeedsCircuit)
	{
		Switch->mOnCircuitsChanged.AddDynamic(
			this,
			&UStructuralPowerSwitchListener::HandleCircuitsChanged);
		bCircuitsBound = true;
	}

	if (bToggleBound && !bNeedsToggle)
	{
		Switch->mOnIsSwitchOnChanged.RemoveDynamic(
			this,
			&UStructuralPowerSwitchListener::HandleSwitchOnChanged);
		bToggleBound = false;
	}
	else if (!bToggleBound && bNeedsToggle)
	{
		Switch->mOnIsSwitchOnChanged.AddDynamic(
			this,
			&UStructuralPowerSwitchListener::HandleSwitchOnChanged);
		bToggleBound = true;
	}
}

void UStructuralPowerSwitchListener::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AFGBuildableCircuitSwitch* Switch = BoundSwitch.Get())
	{
		if (bToggleBound)
		{
			Switch->mOnIsSwitchOnChanged.RemoveDynamic(
				this,
				&UStructuralPowerSwitchListener::HandleSwitchOnChanged);
		}

		if (bCircuitsBound)
		{
			Switch->mOnCircuitsChanged.RemoveDynamic(
				this,
				&UStructuralPowerSwitchListener::HandleCircuitsChanged);
		}
	}

	bToggleBound = false;
	bCircuitsBound = false;

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
	AFGBuildableCircuitSwitch* Switch = BoundSwitch.Get();
	AStructuralPowerGraphSubsystem* Graph = GraphSubsystem.Get();
	if (!IsValid(Graph) || !IsValid(Switch))
	{
		return;
	}

	if (Graph->ShouldDeferCircuitDrivenRefresh())
	{
		return;
	}

	if (Graph->ShouldSkipSwitchCircuitEcho(Switch))
	{
		return;
	}

	UWorld* World = Switch->GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda(
		[GraphWeak = TWeakObjectPtr<AStructuralPowerGraphSubsystem>(Graph),
			SwitchWeak = TWeakObjectPtr<AFGBuildableCircuitSwitch>(Switch)]()
		{
			if (AStructuralPowerGraphSubsystem* GraphPtr = GraphWeak.Get())
			{
				if (AFGBuildableCircuitSwitch* SwitchPtr = SwitchWeak.Get())
				{
					GraphPtr->ProcessSwitchWireDelta(SwitchPtr);
				}
			}
		}));
}
