// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Graph/FOverlapBoxAdjacencyStrategy.h"

#include "Buildables/FGBuildable.h"
#include "Diagnostics/FStructuralPowerTrace.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "FGBuildableSubsystem.h"
#include "Graph/FStructuralAdjacencyHeuristics.h"
#include "Rules/FStructuralEligibilityRules.h"
#include "StructuralPowerConstants.h"
#include "StructuralPowerLog.h"

namespace
{
static void AppendUniqueBusActor(TArray<AFGBuildable*>& Neighbors, AFGBuildable* Candidate)
{
	if (IsValid(Candidate))
	{
		Neighbors.AddUnique(Candidate);
	}
}

static void AppendProximityBusNeighbors(
	UWorld* World,
	AFGBuildable* Source,
	TArray<AFGBuildable*>& Neighbors)
{
	if (!IsValid(Source) || !IsValid(World))
	{
		return;
	}

	AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(World);
	if (!IsValid(BuildableSubsystem))
	{
		return;
	}

	const FBox SourceBounds = FStructuralAdjacencyHeuristics::GetActorAdjacencyBounds(Source);
	const TSubclassOf<AFGBuildable> SourceClass = Source->GetClass();
	int32 ProximityHits = 0;

	for (AFGBuildable* Candidate : BuildableSubsystem->GetAllBuildablesRef())
	{
		if (!IsValid(Candidate) || Candidate == Source)
		{
			continue;
		}

		if (!FStructuralEligibilityRules::IsBusMember(Candidate))
		{
			continue;
		}

		const FBox CandidateBounds = FStructuralAdjacencyHeuristics::GetActorAdjacencyBounds(Candidate);
		if (!FStructuralAdjacencyHeuristics::AreAdjacencyBoundsConnected(
			SourceBounds,
			CandidateBounds,
			SourceClass,
			Candidate->GetClass()))
		{
			continue;
		}

		AppendUniqueBusActor(Neighbors, Candidate);
		++ProximityHits;
	}

	if (ProximityHits > 0 && FStructuralAdjacencyHeuristics::IsBeamBuildable(Source))
	{
		UE_LOG(LogStructuralPower, Verbose,
			TEXT("[PWR] proximity %s added %d bus neighbor(s) (beam bounds gap)"),
			*Source->GetName(),
			ProximityHits);
	}
}
}

void FOverlapBoxAdjacencyStrategy::FindModManagedNeighbors(
	AFGBuildable* Source,
	TArray<AFGBuildable*>& OutNeighbors)
{
	OutNeighbors.Reset();
	if (!IsValid(Source))
	{
		return;
	}

	UWorld* World = Source->GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	const FBox SearchBounds = FStructuralAdjacencyHeuristics::GetActorAdjacencyBounds(Source);
	const FVector Origin = SearchBounds.GetCenter();
	const FVector Extent = SearchBounds.GetExtent();

	const FCollisionShape BoxShape = FCollisionShape::MakeBox(Extent);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(StructuralPowerOverlap), false, Source);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByChannel(
		Overlaps,
		Origin,
		FQuat::Identity,
		ECC_WorldStatic,
		BoxShape,
		QueryParams);

	int32 BusHits = 0;
	int32 SkippedNonBus = 0;

	for (const FOverlapResult& Hit : Overlaps)
	{
		AFGBuildable* Candidate = Cast<AFGBuildable>(Hit.GetActor());
		if (!IsValid(Candidate) || Candidate == Source)
		{
			continue;
		}

		if (!FStructuralEligibilityRules::IsBusMember(Candidate))
		{
			++SkippedNonBus;
			continue;
		}

		++BusHits;
		AppendUniqueBusActor(OutNeighbors, Candidate);
	}

	const int32 OverlapNeighborCount = OutNeighbors.Num();
	if (FStructuralAdjacencyHeuristics::IsBeamBuildable(Source))
	{
		AppendProximityBusNeighbors(World, Source, OutNeighbors);
	}

	FStructuralPowerTrace::LogOverlapQuery(
		Source,
		Overlaps.Num(),
		BusHits,
		OutNeighbors.Num(),
		0);

	if (FStructuralAdjacencyHeuristics::IsBeamBuildable(Source))
	{
		UE_LOG(LogStructuralPower, Verbose,
			TEXT("[PWR] overlap %s raw=%d bus=%d overlapNbr=%d totalNbr=%d padding=%.0f gap=%.0f"),
			*Source->GetName(),
			Overlaps.Num(),
			BusHits,
			OverlapNeighborCount,
			OutNeighbors.Num(),
			StructuralPowerConstants::BeamOverlapPaddingCm,
			StructuralPowerConstants::BeamStructuralGapCm);
	}
}

void FOverlapBoxAdjacencyStrategy::FindBusNeighborsNearBounds(
	UWorld* World,
	const FBox& SearchBounds,
	TSubclassOf<AFGBuildable> SourceClass,
	const AFGBuildable* ExcludeActor,
	TArray<AFGBuildable*>& OutNeighbors)
{
	OutNeighbors.Reset();
	if (!IsValid(World) || !SearchBounds.IsValid)
	{
		return;
	}

	AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(World);
	if (!IsValid(BuildableSubsystem))
	{
		return;
	}

	for (AFGBuildable* Candidate : BuildableSubsystem->GetAllBuildablesRef())
	{
		if (!IsValid(Candidate) || Candidate == ExcludeActor)
		{
			continue;
		}

		if (!FStructuralEligibilityRules::IsBusMember(Candidate))
		{
			continue;
		}

		const FBox CandidateBounds = FStructuralAdjacencyHeuristics::GetActorAdjacencyBounds(Candidate);
		if (FStructuralAdjacencyHeuristics::AreAdjacencyBoundsConnected(
			SearchBounds,
			CandidateBounds,
			SourceClass,
			Candidate->GetClass()))
		{
			OutNeighbors.AddUnique(Candidate);
		}
	}
}
