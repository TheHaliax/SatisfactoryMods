// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Lightweight/FStructuralLightweightTypes.h"

class AFGBuildable;

/** Spatial hash of full-actor bus members for outlet parent queries. */
class FStructuralBusMemberSpatialIndex
{
public:
	void Reset();

	void RebuildFromWorld(class UWorld* World);
	void RegisterMember(AFGBuildable* Buildable);
	void UnregisterMember(AFGBuildable* Buildable);

	FStructuralWallAnchor FindParentForOutlet(AFGBuildable* Outlet) const;

private:
	struct FIndexedMember
	{
		TWeakObjectPtr<AFGBuildable> Buildable;
		FBox WorldBounds;
		TSubclassOf<AFGBuildable> BuildableClass;
	};

	static FIntVector ToCell(const FVector& Location);
	void IndexMemberCells(int32 MemberIndex, const FBox& WorldBounds);
	void UnindexMemberCells(int32 MemberIndex, const FBox& WorldBounds);
	static FBox GetActorBounds(AFGBuildable* Buildable);

	TArray<FIndexedMember> Members;
	TMap<AFGBuildable*, int32> ActorToIndex;
	TMap<FIntVector, TArray<int32>> CellToIndices;
};
