// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Core/FStructuralNodeId.h"
#include "Lightweight/FStructuralLightweightTypes.h"

class AFGBuildable;
class UFGStructuralPowerConnectionComponent;
class UWorld;

/** Spatial index of mod-tracked lightweight structures (placement-only, no world scan). */
class STRUCTURALPOWER_API FStructuralLightweightIndex
{
public:
	int32 GetTrackedCount() const { return Members.Num(); }

	bool RegisterTrackedMember(UWorld* World, const FStructuralLightweightKey& Key);
	void UnregisterMember(const FStructuralLightweightKey& Key);

	FStructuralWallAnchor FindParentWallForOutlet(AFGBuildable* Outlet) const;
	UFGStructuralPowerConnectionComponent* GetOrCreateHiddenConnector(
		UWorld* World,
		const FStructuralLightweightKey& Key,
		const FVector& WorldLocation);
	UFGStructuralPowerConnectionComponent* FindHiddenConnector(const FStructuralLightweightKey& Key) const;

	int32 StitchPlacementNeighbors(
		UWorld* World,
		const FStructuralLightweightKey& Key,
		TFunctionRef<bool(
			UFGStructuralPowerConnectionComponent*,
			UFGStructuralPowerConnectionComponent*,
			const FStructuralLightweightKey&,
			const FStructuralLightweightKey&)> LinkFn);

	void FindModManagedNeighborsInBounds(
		const FBox& WorldBounds,
		TArray<FStructuralLightweightKey>& OutKeys) const;

	void RehydrateConnectorsFromSubsystem(UWorld* World);
	void RegisterSavedMembers(UWorld* World, const TArray<FStructuralLightweightKey>& Keys);

	static FStructuralNodeId MakeNodeId(const FStructuralLightweightKey& Key);
	static FStructuralLightweightKey KeyFromNodeId(const FStructuralNodeId& NodeId);

private:
	struct FIndexedMember
	{
		FStructuralLightweightKey Key;
		FBox WorldBounds;
	};

	static FIntVector ToCell(const FVector& Location);
	void IndexMemberCells(int32 MemberIndex, const FBox& WorldBounds);
	void UnindexMemberCells(int32 MemberIndex, const FBox& WorldBounds);
	const FIndexedMember* FindMember(const FStructuralLightweightKey& Key) const;
	static bool AreBoundsStructurallyConnected(
		const FBox& A,
		const FBox& B,
		TSubclassOf<AFGBuildable> ClassA = nullptr,
		TSubclassOf<AFGBuildable> ClassB = nullptr);
	static FBox GetWorldBounds(
		TSubclassOf<AFGBuildable> BuildableClass,
		const FTransform& Transform,
		const FBox& LocalBounds);

	TArray<FIndexedMember> Members;
	TMap<FStructuralLightweightKey, int32> KeyToIndex;
	TMap<FIntVector, TArray<int32>> CellToIndices;
	TMap<FStructuralLightweightKey, UFGStructuralPowerConnectionComponent*> HiddenConnectors;
};
