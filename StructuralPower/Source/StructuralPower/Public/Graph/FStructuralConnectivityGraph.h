// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Core/FStructuralNodeId.h"
#include "Lightweight/FStructuralLightweightTypes.h"

class AFGBuildable;
class UWorld;

/**
 * Structure-only connectivity: foundations/walls/ramps are pure data here — never power
 * circuit members. Spatial-hash adjacency + union-find groups contiguous structure into
 * components. Bridge poles read their parent's component to decide which pole buses share
 * power. Rebuilt from live geometry on load, maintained incrementally on build/demolish.
 */
class STRUCTURALPOWER_API FStructuralConnectivityGraph
{
public:
	void Reset();

	void Rebuild(UWorld* World);

	/**
	 * Incremental insert. Returns false if the node was already present or ineligible.
	 * OutMergedRoots receives the distinct pre-existing component roots this insert fused
	 * (empty when the node started or extended a single component) so callers can restitch
	 * only the affected pole groups.
	 */
	bool AddActorNode(AFGBuildable* Buildable, TArray<int32>& OutMergedRoots);
	bool AddLightweightNode(UWorld* World, const FStructuralLightweightKey& Key, TArray<int32>& OutMergedRoots);

	/**
	 * Remove a node and locally recompute the component it belonged to. OutAffectedRoots
	 * receives the post-removal component roots that survived (possibly split), so callers
	 * re-energize exactly those pole groups.
	 */
	void RemoveNode(const FStructuralNodeId& Id, TArray<int32>& OutAffectedRoots);

	/** Component representative for a tracked structure; INDEX_NONE if not tracked. */
	int32 FindRoot(const FStructuralNodeId& Id) const;

	/**
	 * Best structural component overlapping the given bounds; INDEX_NONE if none.
	 * When OutBestNodeId is provided it receives the representative structure's id so callers
	 * can persist a stable parent reference (FindRoot(OutBestNodeId) == return value).
	 */
	int32 FindRootForBounds(
		const FBox& Bounds,
		TSubclassOf<AFGBuildable> Class,
		FStructuralNodeId* OutBestNodeId = nullptr) const;

	bool IsTracked(const FStructuralNodeId& Id) const { return IdToIndex.Contains(Id); }
	int32 GetNodeCount() const { return NumValid; }
	void GetComponentStats(int32& OutComponents, int32& OutLargest);

	/** Smallest member node id for a union-find component; invalid if Root is INDEX_NONE. */
	FStructuralNodeId MakeCanonicalNodeIdForComponent(int32 Root) const;

	/** Closest point on any structure bounds within axis limits; for hoverpack (DR-010). */
	bool FindNearestStructureAnchor(
		const FVector& QueryLoc,
		float MaxHorizontal,
		float MaxVertical,
		FVector& OutAnchor,
		int32& OutComponentRoot) const;

private:
	struct FNode
	{
		FStructuralNodeId Id;
		FBox Bounds = FBox(ForceInit);
		TSubclassOf<AFGBuildable> Class = nullptr;
		bool bValid = false;
	};

	static FIntVector ToCell(const FVector& P);
	static FBox ComputeLightweightBounds(
		UWorld* World,
		const FStructuralLightweightKey& Key,
		TSubclassOf<AFGBuildable>& OutClass);

	int32 AllocSlot();
	int32 AddNodeInternal(
		const FStructuralNodeId& Id,
		const FBox& Bounds,
		TSubclassOf<AFGBuildable> Class,
		TArray<int32>& OutMergedRoots);
	void IndexCells(int32 NodeIndex);
	void UnindexCells(int32 NodeIndex);
	void CollectNeighbors(int32 NodeIndex, TArray<int32>& Out) const;
	void CollectComponent(int32 StartIndex, TArray<int32>& Out) const;
	int32 Find(int32 Index);
	int32 FindRootIndexReadOnly(int32 Index) const;
	void Union(int32 A, int32 B);

	TArray<FNode> Nodes;
	TArray<int32> Parent;
	TArray<int32> RankArr;
	TArray<int32> FreeSlots;
	TMap<FStructuralNodeId, int32> IdToIndex;
	TMap<FIntVector, TArray<int32>> Cells;
	int32 NumValid = 0;
};
