// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Core/FStructuralNodeId.h"
#include "Lightweight/FStructuralLightweightTypes.h"

class AFGBuildable;
class UWorld;

class STRUCTURALPOWER_API FStructuralLightweightIndex
{
public:
	int32 GetTrackedCount() const { return Members.Num(); }

	bool RegisterTrackedMember(UWorld* World, const FStructuralLightweightKey& Key);
	void UnregisterMember(const FStructuralLightweightKey& Key);

	FStructuralWallAnchor FindParentWallForOutlet(AFGBuildable* Outlet) const;

	static bool AreBoundsStructurallyConnected(
		const FBox& A,
		const FBox& B,
		TSubclassOf<AFGBuildable> ClassA = nullptr,
		TSubclassOf<AFGBuildable> ClassB = nullptr);

	static FStructuralNodeId MakeNodeId(const FStructuralLightweightKey& Key);

private:
	struct FIndexedMember
	{
		FStructuralLightweightKey Key;
		FBox WorldBounds;
	};

	static FIntVector ToCell(const FVector& Location);
	void IndexMemberCells(int32 MemberIndex, const FBox& WorldBounds);
	void UnindexMemberCells(int32 MemberIndex, const FBox& WorldBounds);
	static FBox GetWorldBounds(
		TSubclassOf<AFGBuildable> BuildableClass,
		const FTransform& Transform,
		const FBox& LocalBounds);

	TArray<FIndexedMember> Members;
	TMap<FStructuralLightweightKey, int32> KeyToIndex;
	TMap<FIntVector, TArray<int32>> CellToIndices;
};
