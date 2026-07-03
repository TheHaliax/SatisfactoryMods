// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class AFGBuildable;
class UFGPowerConnectionComponent;
class UWorld;

/** Strategy: overlap box adjacency (guidelines doc). */
class STRUCTURALPOWER_API FOverlapBoxAdjacencyStrategy
{
public:
	static void FindModManagedNeighbors(
		AFGBuildable* Source,
		TArray<AFGBuildable*>& OutNeighbors);

	static void FindBusNeighborsNearBounds(
		UWorld* World,
		const FBox& SearchBounds,
		TSubclassOf<AFGBuildable> SourceClass,
		const AFGBuildable* ExcludeActor,
		TArray<AFGBuildable*>& OutNeighbors);
};
