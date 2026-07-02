// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Graph/FOverlapBoxAdjacencyStrategy.h"

#include "Buildables/FGBuildable.h"
#include "Components/UFGStructuralPowerConnectionComponent.h"
#include "Diagnostics/FStructuralPowerTrace.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Rules/FStructuralEligibilityRules.h"
#include "Save/AStructuralPowerGraphSubsystem.h"
#include "StructuralPowerConstants.h"

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

	FVector Origin;
	FVector Extent;
	Source->GetActorBounds(false, Origin, Extent);
	Extent += FVector(StructuralPowerConstants::OverlapPaddingCm);

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
	int32 NoConnectorHits = 0;

	for (const FOverlapResult& Hit : Overlaps)
	{
		AFGBuildable* Candidate = Cast<AFGBuildable>(Hit.GetActor());
		if (!IsValid(Candidate) || Candidate == Source)
		{
			continue;
		}

		if (!FStructuralEligibilityRules::IsBusMember(Candidate))
		{
			continue;
		}

		++BusHits;

		if (!AStructuralPowerGraphSubsystem::HasStructuralConnector(Candidate))
		{
			++NoConnectorHits;
			continue;
		}

		OutNeighbors.AddUnique(Candidate);
	}

	FStructuralPowerTrace::LogOverlapQuery(
		Source,
		Overlaps.Num(),
		BusHits,
		OutNeighbors.Num(),
		NoConnectorHits);
}
