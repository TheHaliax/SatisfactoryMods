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
/** Bottom of pole bounds for ground/tower outlets; actor location for wall outlets. */
FVector GetOutletAnchorLocation(const AFGBuildable* Outlet);
/** Footprint preferred; else within MaxOutletParentDistCm + vertical band + horizontal cap. */
bool IsOutletNearBounds(const FBox& Bounds, TSubclassOf<AFGBuildable> BuildableClass, const AFGBuildable* Outlet);
/** Lower score wins. Wall outlets penalize foundation/corner-over-floor picks. */
float ScoreParentCandidate(
	const FVector& AnchorLocation,
	const FBox& Bounds,
	TSubclassOf<AFGBuildable> BuildableClass,
	const AFGBuildable* Outlet);
}
