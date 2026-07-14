// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Diagnostics/FStructuralPowerDiagnostics.h"

#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "Components/UFGStructuralPowerConnectionComponent.h"
#include "Core/FStructuralPowerWorldGate.h"
#include "Diagnostics/FStructuralPowerTrace.h"
#include "FGBuildableSubsystem.h"
#include "Rules/FStructuralEligibilityRules.h"
#include "Save/AStructuralPowerGraphSubsystem.h"
#include "Session/FStructuralPowerSessionSettings.h"
#include "StructuralPowerLog.h"

void FStructuralPowerDiagnostics::AuditWorld(UWorld* World, bool bAllowMenuWorld)
{
	if (!IsValid(World))
	{
		return;
	}

	if (!bAllowMenuWorld && FStructuralPowerWorldGate::IsMenuWorld(World))
	{
		return;
	}

	AStructuralPowerGraphSubsystem* Graph = AStructuralPowerGraphSubsystem::GetOrCreate(World);

	AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(World);
	if (!IsValid(BuildableSubsystem))
	{
		return;
	}

	if (Graph)
	{
		int32 Components = 0;
		int32 Largest = 0;
		Graph->GetGraphStats(Components, Largest);

		UE_LOG(LogStructuralPower, Log,
			TEXT("Graph: %d structure node(s), %d component(s) (largest %d), %d tracked pole(s), %d LW tracked, %d pending, propagation=%d trace=%d"),
			Graph->GetStructureNodeCount(),
			Components,
			Largest,
			Graph->GetTrackedPoleCount(),
			Graph->GetTrackedLightweightCount(),
			Graph->GetPendingJobCount(),
			FStructuralPowerSessionSettings::IsPropagationEnabled() ? 1 : 0,
			FStructuralPowerTrace::IsEnabled() ? 1 : 0);
	}

	int32 PolesTotal = 0;
	int32 PolesWithBus = 0;
	int32 PolesInCircuit = 0;
	int32 PolesNoBus = 0;

	for (AFGBuildable* Buildable : BuildableSubsystem->GetAllBuildablesRef())
	{
		if (!FStructuralEligibilityRules::IsPowerBridgePole(Buildable))
		{
			continue;
		}

		++PolesTotal;
		AFGBuildablePowerPole* Pole = Cast<AFGBuildablePowerPole>(Buildable);

		UFGStructuralPowerConnectionComponent* OutletBus =
			AStructuralPowerGraphSubsystem::FindOutletBusConnector(Pole);
		if (!OutletBus)
		{
			++PolesNoBus;
			continue;
		}

		++PolesWithBus;
		if (OutletBus->GetCircuitID() != INDEX_NONE)
		{
			++PolesInCircuit;
		}
	}

	UE_LOG(LogStructuralPower, Log, TEXT("=== StructuralPower Diagnostics ==="));
	UE_LOG(LogStructuralPower, Log,
		TEXT("Bridge poles: %d total | WITH_BUS: %d | IN_CIRCUIT: %d | NO_BUS: %d"),
		PolesTotal,
		PolesWithBus,
		PolesInCircuit,
		PolesNoBus);
	UE_LOG(LogStructuralPower, Log,
		TEXT("Pole-to-pole: WITH_BUS = tracked pole (bus created). IN_CIRCUIT = bus promoted to a power circuit (component powered)."));
}
