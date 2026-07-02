// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Diagnostics/FStructuralPowerDiagnostics.h"

#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "Components/UFGStructuralPowerConnectionComponent.h"
#include "Diagnostics/FStructuralPowerTrace.h"
#include "FGBuildableSubsystem.h"
#include "FGPowerConnectionComponent.h"
#include "Lightweight/FStructuralLightweightTypes.h"
#include "Rules/FStructuralEligibilityRules.h"
#include "Save/AStructuralPowerGraphSubsystem.h"
#include "Session/FStructuralPowerSessionSettings.h"
#include "StructuralPowerLog.h"

namespace
{
bool ShouldSkipAutomaticAudit(const UWorld* World)
{
	if (!IsValid(World))
	{
		return true;
	}

	const FString MapName = World->GetMapName();
	return MapName.Contains(TEXT("Menu"), ESearchCase::IgnoreCase);
}
}

void FStructuralPowerDiagnostics::AuditWorld(UWorld* World, bool bAllowMenuWorld)
{
	if (!IsValid(World))
	{
		return;
	}

	if (!bAllowMenuWorld && ShouldSkipAutomaticAudit(World))
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
		UE_LOG(LogStructuralPower, Log,
			TEXT("Graph: %d runtime nodes, %d tracked LW, %d pending jobs, propagation=%d trace=%d"),
			Graph->GetRuntimeNodeCount(),
			Graph->GetTrackedLightweightCount(),
			Graph->GetPendingJobCount(),
			FStructuralPowerSessionSettings::IsPropagationEnabled() ? 1 : 0,
			FStructuralPowerTrace::IsEnabled() ? 1 : 0);
	}

	int32 OutletsTotal = 0;
	int32 OutletsOk = 0;
	int32 NoParent = 0;
	int32 NoComponent = 0;
	int32 NoCircuit = 0;
	int32 LegacyUntracked = 0;

	for (AFGBuildable* Buildable : BuildableSubsystem->GetAllBuildablesRef())
	{
		if (!FStructuralEligibilityRules::IsPowerBridgePole(Buildable))
		{
			continue;
		}

		++OutletsTotal;
		AFGBuildablePowerPole* Pole = Cast<AFGBuildablePowerPole>(Buildable);

		UFGStructuralPowerConnectionComponent* OutletBus =
			AStructuralPowerGraphSubsystem::FindOutletBusConnector(Pole);
		if (!OutletBus)
		{
			++LegacyUntracked;
		}

		const FStructuralWallAnchor ParentAnchor = Graph
			? Graph->ResolveOutletAnchor(Buildable)
			: FStructuralWallAnchor{};
		UFGStructuralPowerConnectionComponent* ParentHidden = Graph
			? Graph->FindStructureHidden(ParentAnchor)
			: nullptr;

		if (!ParentAnchor.IsValid())
		{
			++NoParent;
			continue;
		}

		if (!OutletBus || !ParentHidden)
		{
			++NoComponent;
			continue;
		}

		const int32 BusCircuit = OutletBus->GetCircuitID();
		const int32 ParentCircuit = ParentHidden->GetCircuitID();
		if (BusCircuit == INDEX_NONE || ParentCircuit == INDEX_NONE || BusCircuit != ParentCircuit)
		{
			++NoCircuit;
			continue;
		}

		++OutletsOk;
	}

	UE_LOG(LogStructuralPower, Log, TEXT("=== StructuralPower Diagnostics ==="));
	UE_LOG(LogStructuralPower, Log,
		TEXT("Bridge poles: %d total | OK: %d | NO_PARENT: %d | NO_COMPONENT: %d | NO_CIRCUIT: %d | LEGACY_UNTRACKED: %d"),
		OutletsTotal,
		OutletsOk,
		NoParent,
		NoComponent,
		NoCircuit,
		LegacyUntracked);
	UE_LOG(LogStructuralPower, Log,
		TEXT("Placement-only: mod tracks structures/bridge poles placed after install. Legacy pre-mod geometry skipped."));
	UE_LOG(LogStructuralPower, Log,
		TEXT("NO_COMPONENT = pole with structural parent but incomplete mod wiring (e.g. isolated foundation test)."));
}
