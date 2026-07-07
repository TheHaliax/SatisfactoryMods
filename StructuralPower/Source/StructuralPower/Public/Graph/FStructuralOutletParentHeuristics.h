// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Buildables/FGBuildable.h"

namespace FStructuralOutletParentHeuristics
{
bool IsPreferredWallClass(const AFGBuildable* Buildable);
bool IsPreferredFoundationClass(const AFGBuildable* Buildable);
bool IsWallOutlet(const AFGBuildable* Outlet);
bool PrefersFoundationAnchor(const AFGBuildable* Outlet);
FVector GetOutletAnchorLocation(const AFGBuildable* Outlet);
bool IsOutletNearBounds(const FBox& Bounds, TSubclassOf<AFGBuildable> BuildableClass, const AFGBuildable* Outlet);
float ScoreParentCandidate(
	const FVector& AnchorLocation,
	const FBox& Bounds,
	TSubclassOf<AFGBuildable> BuildableClass,
	const AFGBuildable* Outlet);
}
