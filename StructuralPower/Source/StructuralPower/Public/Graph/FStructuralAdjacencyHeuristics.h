// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Buildables/FGBuildable.h"

class AFGBuildable;

namespace FStructuralAdjacencyHeuristics
{
bool IsBeamBuildable(const AFGBuildable* Buildable);
bool IsBeamClass(TSubclassOf<AFGBuildable> BuildableClass);
FBox GetActorAdjacencyBounds(const AFGBuildable* Buildable);
float GetStructuralGapCm(TSubclassOf<AFGBuildable> ClassA, TSubclassOf<AFGBuildable> ClassB);
bool AreAdjacencyBoundsConnected(
	const FBox& BoundsA,
	const FBox& BoundsB,
	TSubclassOf<AFGBuildable> ClassA,
	TSubclassOf<AFGBuildable> ClassB);
float ComputeAdjacencyBoundsDistCm(const FBox& BoundsA, const FBox& BoundsB);
}
