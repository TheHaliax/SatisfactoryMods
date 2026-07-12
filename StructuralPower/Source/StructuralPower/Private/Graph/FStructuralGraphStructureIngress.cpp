// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Graph/FStructuralGraphStructureIngress.h"

#include "Core/FStructuralGraphSession.h"

#include "Buildables/FGBuildable.h"
#include "Diagnostics/FStructuralPowerTrace.h"
#include "Engine/World.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Lightweight/FStructuralLightweightIndex.h"
#include "Lightweight/FStructuralLightweightTypes.h"
#include "Rules/FStructuralEligibilityRules.h"
#include "Save/FStructuralPlacementQueue.h"
#include "StructuralPowerLog.h"

void FStructuralGraphStructureIngress::Bind(FStructuralGraphSession* InSession)
{
	Session = InSession;
}

void FStructuralGraphStructureIngress::ProcessStructure(AFGBuildable* Buildable)
{
	if (!Session)
	{
		return;
	}

	if (!FStructuralEligibilityRules::IsBusMember(Buildable))
	{
		FStructuralPowerTrace::LogPlacementSkip(Buildable, TEXT("not_bus_member"));
		return;
	}

	Session->RegisterBuildableActor(Buildable);

	TArray<int32> MergedRoots;
	if (!Session->StructureGraph().AddActorNode(Buildable, MergedRoots))
	{
		return;
	}

	if (MergedRoots.Num() >= 2)
	{
		const int32 Root = Session->StructureGraph().FindRoot(Session->MakeNodeId(Buildable));

		UE_LOG(LogStructuralPower, Verbose,
			TEXT("[HALSP] structure %s fused %d component(s) -> root %d"),
			*Buildable->GetName(),
			MergedRoots.Num(),
			Root);
	}

	RetryAwaitingStructuralSiteEndpoints();
}

void FStructuralGraphStructureIngress::ProcessLightweightStructure(
	const FStructuralLightweightKey& Key)
{
	if (!Session || !Key.IsValid())
	{
		return;
	}

	UWorld* World = Session->GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	Session->LightweightIndex().RegisterTrackedMember(World, Key);

	TArray<int32> MergedRoots;
	if (!Session->StructureGraph().AddLightweightNode(World, Key, MergedRoots))
	{
		return;
	}

	if (MergedRoots.Num() >= 2)
	{
		(void)Session->StructureGraph().FindRoot(FStructuralLightweightIndex::MakeNodeId(Key));
	}

	RetryAwaitingStructuralSiteEndpoints();
}

void FStructuralGraphStructureIngress::RetryAwaitingStructuralSiteEndpoints()
{
	for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : Session->TrackedEndpoints())
	{
		if (!Pair.Value.bAwaitingStructuralSite)
		{
			continue;
		}

		if (Pair.Value.Kind != EStructuralEndpointKind::Pole
			&& Pair.Value.Kind != EStructuralEndpointKind::Switch
			&& Pair.Value.Kind != EStructuralEndpointKind::Storage)
		{
			continue;
		}

		if (AFGBuildable* Buildable = Pair.Value.Actor.Get())
		{
			Session->EnqueuePlacement(Buildable, EStructuralPlacementJobType::Outlet, /*bDefer=*/true);
		}
	}
}
