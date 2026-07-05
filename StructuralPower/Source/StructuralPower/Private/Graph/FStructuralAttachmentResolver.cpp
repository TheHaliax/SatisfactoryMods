// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Graph/FStructuralAttachmentResolver.h"

#include "Buildables/FGBuildable.h"
#include "Graph/FStructuralAdjacencyHeuristics.h"
#include "Graph/FStructuralConnectivityGraph.h"
#include "Graph/FStructuralOutletParentResolver.h"
#include "Lightweight/FStructuralLightweightIndex.h"

FStructuralWallAnchor FStructuralAttachmentResolver::ResolveStructuralParent(
	AFGBuildable* Buildable,
	UWorld* World,
	const FStructuralLightweightIndex& LightweightIndex)
{
	return FStructuralOutletParentResolver::Resolve(Buildable, World, LightweightIndex);
}

FStructuralComponentResolveResult FStructuralAttachmentResolver::ResolveStructuralComponent(
	const FStructuralConnectivityGraph& Graph,
	const FVector& WorldLoc,
	float QueryRadiusCm,
	TSubclassOf<AFGBuildable> ClassHint)
{
	FStructuralComponentResolveResult Result;
	if (QueryRadiusCm <= 0.0f)
	{
		return Result;
	}

	const FVector Extent(QueryRadiusCm);
	const FBox QueryBox(WorldLoc - Extent, WorldLoc + Extent);
	Result.ComponentRoot = Graph.FindRootForBounds(QueryBox, ClassHint, &Result.NodeId);
	return Result;
}

int32 FStructuralAttachmentResolver::ResolveComponentRootForBuildable(
	AFGBuildable* Buildable,
	const FStructuralConnectivityGraph& Graph,
	const FStructuralLightweightIndex& LightweightIndex,
	UWorld* World,
	FStructuralNodeId& OutParentId)
{
	OutParentId = FStructuralNodeId();
	if (!IsValid(Buildable))
	{
		return INDEX_NONE;
	}

	const FStructuralWallAnchor Anchor = ResolveStructuralParent(Buildable, World, LightweightIndex);
	if (Anchor.IsValid())
	{
		FStructuralNodeId ParentId;
		if (IsValid(Anchor.Actor))
		{
			ParentId.BuildableClass = FSoftClassPath(Anchor.Actor->GetClass());
			ParentId.ActorName = Anchor.Actor->GetFName();
		}
		else if (Anchor.Lightweight.IsValid())
		{
			ParentId = FStructuralLightweightIndex::MakeNodeId(Anchor.Lightweight);
		}

		const int32 Root = Graph.FindRoot(ParentId);
		if (Root != INDEX_NONE)
		{
			OutParentId = ParentId;
			return Root;
		}
	}

	const FBox Bounds = FStructuralAdjacencyHeuristics::GetActorAdjacencyBounds(Buildable);
	return Graph.FindRootForBounds(Bounds, Buildable->GetClass(), &OutParentId);
}
