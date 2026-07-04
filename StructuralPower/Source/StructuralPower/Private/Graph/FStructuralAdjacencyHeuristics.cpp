// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Graph/FStructuralAdjacencyHeuristics.h"

#include "FGBuildableBeam.h"
#include "StructuralPowerConstants.h"

namespace FStructuralAdjacencyHeuristics
{
bool IsBeamClass(TSubclassOf<AFGBuildable> BuildableClass)
{
	if (!BuildableClass)
	{
		return false;
	}

	const AFGBuildable* CDO = BuildableClass->GetDefaultObject<AFGBuildable>();
	return IsBeamBuildable(CDO);
}

bool IsBeamBuildable(const AFGBuildable* Buildable)
{
	if (!IsValid(Buildable))
	{
		return false;
	}

	if (Buildable->IsA<AFGBuildableBeam>())
	{
		return true;
	}

	return Buildable->GetClass()->GetName().Contains(TEXT("SteelBeam"), ESearchCase::IgnoreCase);
}

FBox GetActorAdjacencyBounds(const AFGBuildable* Buildable)
{
	if (!IsValid(Buildable))
	{
		return FBox(ForceInit);
	}

	FVector Origin;
	FVector Extent;
	const_cast<AFGBuildable*>(Buildable)->GetActorBounds(false, Origin, Extent);

	const float Padding = IsBeamBuildable(Buildable)
		? StructuralPowerConstants::BeamOverlapPaddingCm
		: StructuralPowerConstants::OverlapPaddingCm;

	return FBox(Origin - Extent - Padding, Origin + Extent + Padding);
}

float GetStructuralGapCm(TSubclassOf<AFGBuildable> ClassA, TSubclassOf<AFGBuildable> ClassB)
{
	if (IsBeamClass(ClassA) || IsBeamClass(ClassB))
	{
		return StructuralPowerConstants::BeamStructuralGapCm;
	}

	return StructuralPowerConstants::StructuralConnectivityGapCm;
}

bool AreAdjacencyBoundsConnected(
	const FBox& BoundsA,
	const FBox& BoundsB,
	TSubclassOf<AFGBuildable> ClassA,
	TSubclassOf<AFGBuildable> ClassB)
{
	if (!BoundsA.IsValid || !BoundsB.IsValid)
	{
		return false;
	}

	const float Gap = GetStructuralGapCm(ClassA, ClassB);
	const FBox ExpandedA = BoundsA.ExpandBy(Gap);
	const FBox ExpandedB = BoundsB.ExpandBy(Gap);

	if (ExpandedA.Intersect(ExpandedB))
	{
		return true;
	}

	return ExpandedA.ComputeSquaredDistanceToBox(BoundsB) <= Gap * Gap;
}
}
