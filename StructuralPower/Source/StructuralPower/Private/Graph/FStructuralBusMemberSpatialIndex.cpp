// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Graph/FStructuralBusMemberSpatialIndex.h"

#include "Buildables/FGBuildable.h"
#include "Engine/World.h"
#include "FGBuildableSubsystem.h"
#include "Graph/FStructuralOutletParentHeuristics.h"
#include "Rules/FStructuralEligibilityRules.h"
#include "StructuralPowerConstants.h"

FIntVector FStructuralBusMemberSpatialIndex::ToCell(const FVector& Location)
{
	return FIntVector(
		FMath::FloorToInt(Location.X / StructuralPowerConstants::GridCellCm),
		FMath::FloorToInt(Location.Y / StructuralPowerConstants::GridCellCm),
		FMath::FloorToInt(Location.Z / StructuralPowerConstants::GridCellCm));
}

FBox FStructuralBusMemberSpatialIndex::GetActorBounds(AFGBuildable* Buildable)
{
	if (!IsValid(Buildable))
	{
		return FBox(ForceInit);
	}

	FVector Origin;
	FVector Extent;
	Buildable->GetActorBounds(false, Origin, Extent);
	return FBox(Origin - Extent, Origin + Extent);
}

void FStructuralBusMemberSpatialIndex::Reset()
{
	Members.Reset();
	ActorToIndex.Reset();
	CellToIndices.Reset();
}

void FStructuralBusMemberSpatialIndex::IndexMemberCells(int32 MemberIndex, const FBox& WorldBounds)
{
	if (!WorldBounds.IsValid)
	{
		return;
	}

	const FIntVector MinCell = ToCell(WorldBounds.Min);
	const FIntVector MaxCell = ToCell(WorldBounds.Max);
	for (int32 X = MinCell.X; X <= MaxCell.X; ++X)
	{
		for (int32 Y = MinCell.Y; Y <= MaxCell.Y; ++Y)
		{
			for (int32 Z = MinCell.Z; Z <= MaxCell.Z; ++Z)
			{
				CellToIndices.FindOrAdd(FIntVector(X, Y, Z)).AddUnique(MemberIndex);
			}
		}
	}
}

void FStructuralBusMemberSpatialIndex::UnindexMemberCells(int32 MemberIndex, const FBox& WorldBounds)
{
	if (!WorldBounds.IsValid)
	{
		return;
	}

	const FIntVector MinCell = ToCell(WorldBounds.Min);
	const FIntVector MaxCell = ToCell(WorldBounds.Max);
	for (int32 X = MinCell.X; X <= MaxCell.X; ++X)
	{
		for (int32 Y = MinCell.Y; Y <= MaxCell.Y; ++Y)
		{
			for (int32 Z = MinCell.Z; Z <= MaxCell.Z; ++Z)
			{
				if (TArray<int32>* Indices = CellToIndices.Find(FIntVector(X, Y, Z)))
				{
					Indices->Remove(MemberIndex);
					if (Indices->Num() == 0)
					{
						CellToIndices.Remove(FIntVector(X, Y, Z));
					}
				}
			}
		}
	}
}

void FStructuralBusMemberSpatialIndex::RegisterMember(AFGBuildable* Buildable)
{
	if (!IsValid(Buildable) || !FStructuralEligibilityRules::IsBusMember(Buildable))
	{
		return;
	}

	if (const int32* ExistingIndex = ActorToIndex.Find(Buildable))
	{
		if (Members.IsValidIndex(*ExistingIndex))
		{
			UnindexMemberCells(*ExistingIndex, Members[*ExistingIndex].WorldBounds);
		}
		ActorToIndex.Remove(Buildable);
	}

	const FBox Bounds = GetActorBounds(Buildable);
	if (!Bounds.IsValid)
	{
		return;
	}

	FIndexedMember Member;
	Member.Buildable = Buildable;
	Member.WorldBounds = Bounds;
	Member.BuildableClass = Buildable->GetClass();

	const int32 MemberIndex = Members.Add(Member);
	ActorToIndex.Add(Buildable, MemberIndex);
	IndexMemberCells(MemberIndex, Bounds);
}

void FStructuralBusMemberSpatialIndex::UnregisterMember(AFGBuildable* Buildable)
{
	const int32* MemberIndexPtr = ActorToIndex.Find(Buildable);
	if (!MemberIndexPtr || !Members.IsValidIndex(*MemberIndexPtr))
	{
		ActorToIndex.Remove(Buildable);
		return;
	}

	const int32 RemoveIndex = *MemberIndexPtr;
	UnindexMemberCells(RemoveIndex, Members[RemoveIndex].WorldBounds);

	const int32 LastIndex = Members.Num() - 1;
	if (RemoveIndex != LastIndex)
	{
		Members.Swap(RemoveIndex, LastIndex);
		if (AFGBuildable* Swapped = Members[RemoveIndex].Buildable.Get())
		{
			ActorToIndex[Swapped] = RemoveIndex;
		}
	}

	Members.SetNum(LastIndex);
	ActorToIndex.Remove(Buildable);
}

void FStructuralBusMemberSpatialIndex::RebuildFromWorld(UWorld* World)
{
	Reset();
	if (!IsValid(World))
	{
		return;
	}

	AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(World);
	if (!IsValid(BuildableSubsystem))
	{
		return;
	}

	for (AFGBuildable* Buildable : BuildableSubsystem->GetAllBuildablesRef())
	{
		RegisterMember(Buildable);
	}
}

FStructuralWallAnchor FStructuralBusMemberSpatialIndex::FindParentForOutlet(AFGBuildable* Outlet) const
{
	if (!IsValid(Outlet) || Members.Num() == 0)
	{
		return {};
	}

	const FVector AnchorLocation = FStructuralOutletParentHeuristics::GetOutletAnchorLocation(Outlet);
	const FIntVector Origin = ToCell(AnchorLocation);
	const int32 CellRadius = FMath::CeilToInt(
		StructuralPowerConstants::MaxOutletParentDistCm / StructuralPowerConstants::GridCellCm);

	const bool bPreferFoundation = FStructuralOutletParentHeuristics::PrefersFoundationAnchor(Outlet);

	FStructuralWallAnchor BestPreferred;
	float BestPreferredScore = TNumericLimits<float>::Max();
	FStructuralWallAnchor BestAny;
	float BestAnyScore = TNumericLimits<float>::Max();

	for (int32 DX = -CellRadius; DX <= CellRadius; ++DX)
	{
		for (int32 DY = -CellRadius; DY <= CellRadius; ++DY)
		{
			for (int32 DZ = -CellRadius; DZ <= CellRadius; ++DZ)
			{
				const TArray<int32>* Indices = CellToIndices.Find(Origin + FIntVector(DX, DY, DZ));
				if (!Indices)
				{
					continue;
				}

				for (int32 MemberIndex : *Indices)
				{
					const FIndexedMember& Member = Members[MemberIndex];
					AFGBuildable* Candidate = Member.Buildable.Get();
					if (!IsValid(Candidate) || Candidate == Outlet)
					{
						continue;
					}

					if (!FStructuralOutletParentHeuristics::IsOutletNearBounds(
						Member.WorldBounds,
						Member.BuildableClass,
						Outlet))
					{
						continue;
					}

					FStructuralWallAnchor Anchor;
					Anchor.Actor = Candidate;
					Anchor.WorldLocation = Member.WorldBounds.GetClosestPointTo(AnchorLocation);

					const float Score = FStructuralOutletParentHeuristics::ScoreParentCandidate(
						AnchorLocation,
						Member.WorldBounds,
						Member.BuildableClass,
						Outlet);

					const AFGBuildable* CDO = Member.BuildableClass
						? Member.BuildableClass->GetDefaultObject<AFGBuildable>()
						: nullptr;
					const bool bPreferredClass = bPreferFoundation
						? FStructuralOutletParentHeuristics::IsPreferredFoundationClass(CDO)
						: FStructuralOutletParentHeuristics::IsPreferredWallClass(CDO);

					if (bPreferredClass && Score < BestPreferredScore)
					{
						BestPreferredScore = Score;
						BestPreferred = Anchor;
					}
					else if (Score < BestAnyScore)
					{
						BestAnyScore = Score;
						BestAny = Anchor;
					}
				}
			}
		}
	}

	return BestPreferred.IsValid() ? BestPreferred : BestAny;
}
