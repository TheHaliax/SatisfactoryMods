// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "Buildables/FGBuildable.h"
#include "CoreMinimal.h"

class AFGBuildable;

namespace FStructuralAdjacencyHeuristics {
bool IsBeamBuildable(const AFGBuildable* Buildable);
bool IsBeamClass(TSubclassOf<AFGBuildable> BuildableClass);
FBox GetActorAdjacencyBounds(const AFGBuildable* Buildable);
float GetStructuralGapCm(TSubclassOf<AFGBuildable> ClassA, TSubclassOf<AFGBuildable> ClassB);
bool AreAdjacencyBoundsConnected(const FBox& BoundsA, const FBox& BoundsB,
                                 TSubclassOf<AFGBuildable> ClassA,
                                 TSubclassOf<AFGBuildable> ClassB);
}
