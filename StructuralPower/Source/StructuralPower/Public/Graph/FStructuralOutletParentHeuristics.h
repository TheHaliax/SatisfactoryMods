// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "Buildables/FGBuildable.h"
#include "CoreMinimal.h"

namespace FStructuralOutletParentHeuristics {
bool IsPreferredWallClass(const AFGBuildable* Buildable);
bool IsPreferredFoundationClass(const AFGBuildable* Buildable);
bool IsFactoryBuildingClass(const AFGBuildable* Buildable);
FBox ExpandFactoryBuildingBounds(const FBox& Bounds);
bool IsEndpointOnFactoryBuildingTop(const FVector& AnchorLocation, const FBox& Bounds);
bool IsWallOutlet(const AFGBuildable* Outlet);
bool PrefersFoundationAnchor(const AFGBuildable* Outlet);
FVector GetOutletAnchorLocation(const AFGBuildable* Outlet);
bool IsOutletNearBounds(const FBox& Bounds, TSubclassOf<AFGBuildable> BuildableClass,
                        const AFGBuildable* Outlet);
bool IsGroundPoleOnFoundationFootprint(const FVector& AnchorLocation, const FBox& Bounds,
                                       TSubclassOf<AFGBuildable> BuildableClass);
bool IsConfirmedStructuralAttach(const AFGBuildable* Outlet, const FBox& ParentBounds,
                                 TSubclassOf<AFGBuildable> ParentClass);
float ScoreParentCandidate(const FVector& AnchorLocation, const FBox& Bounds,
                           TSubclassOf<AFGBuildable> BuildableClass, const AFGBuildable* Outlet);
}
