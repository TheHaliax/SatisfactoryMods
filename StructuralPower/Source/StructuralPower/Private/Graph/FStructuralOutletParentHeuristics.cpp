// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Graph/FStructuralOutletParentHeuristics.h"

#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Buildables/FGBuildableCornerWall.h"
#include "Buildables/FGBuildableFactoryBuilding.h"
#include "Buildables/FGBuildableFoundation.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "Buildables/FGBuildableRamp.h"
#include "Buildables/FGBuildableWall.h"
#include "Rules/FStructuralEligibilityRules.h"
#include "StructuralPowerConstants.h"

namespace {
static bool IsWallMountedSwitch(const AFGBuildable* Outlet) {
  const AFGBuildableCircuitSwitch* Switch = Cast<AFGBuildableCircuitSwitch>(Outlet);
  if (!IsValid(Switch)) {
    return false;
  }

  return FMath::Abs(Switch->GetActorUpVector().Z) < 0.5f;
}

static bool IsFactoryBuildingClass(const AFGBuildable* Buildable) {
  return IsValid(Buildable) && Buildable->IsA<AFGBuildableFactoryBuilding>();
}

static bool IsPreferredWallClass(const AFGBuildable* Buildable) {
  return Buildable->IsA<AFGBuildableWall>() || Buildable->IsA<AFGBuildableCornerWall>();
}

static bool IsPreferredFoundationClass(const AFGBuildable* Buildable) {
  return Buildable->IsA<AFGBuildableFoundation>() || Buildable->IsA<AFGBuildableRamp>();
}

static bool IsGroundPoleOnFoundationFootprint(const FVector& AnchorLocation, const FBox& Bounds,
                                              TSubclassOf<AFGBuildable> BuildableClass) {
  if (!Bounds.IsValid || !BuildableClass) {
    return false;
  }

  const AFGBuildable* CDO = BuildableClass->GetDefaultObject<AFGBuildable>();
  if (!IsPreferredFoundationClass(CDO)) {
    return false;
  }

  const float Gap = StructuralPowerConstants::StructuralConnectivityGapCm;
  if (AnchorLocation.X < (Bounds.Min.X - Gap) || AnchorLocation.X > (Bounds.Max.X + Gap) ||
      AnchorLocation.Y < (Bounds.Min.Y - Gap) || AnchorLocation.Y > (Bounds.Max.Y + Gap)) {
    return false;
  }

  const float TopZ = Bounds.Max.Z;
  return AnchorLocation.Z >= (TopZ - Gap) &&
         AnchorLocation.Z <= (TopZ + StructuralPowerConstants::GroundPoleFoundationVerticalReachCm);
}

static bool IsGroundPoleInFoundationVerticalBand(const FVector& AnchorLocation, const FBox& Bounds,
                                                 TSubclassOf<AFGBuildable> BuildableClass) {
  if (!Bounds.IsValid || !BuildableClass) {
    return false;
  }

  const AFGBuildable* CDO = BuildableClass->GetDefaultObject<AFGBuildable>();
  if (!IsPreferredFoundationClass(CDO)) {
    return false;
  }

  const float TopZ = Bounds.Max.Z;
  const float Gap = StructuralPowerConstants::StructuralConnectivityGapCm;
  const float Below = StructuralPowerConstants::GroundPoleFoundationVerticalBandBelowCm;
  return AnchorLocation.Z >= (TopZ - Gap - Below) &&
         AnchorLocation.Z <= (TopZ + StructuralPowerConstants::GroundPoleFoundationVerticalReachCm);
}

static bool IsGroundPoleParentCandidate(const FVector& AnchorLocation, const FBox& Bounds,
                                        TSubclassOf<AFGBuildable> BuildableClass) {
  if (!Bounds.IsValid || !BuildableClass) {
    return false;
  }

  if (IsGroundPoleOnFoundationFootprint(AnchorLocation, Bounds, BuildableClass)) {
    return true;
  }

  if (!IsGroundPoleInFoundationVerticalBand(AnchorLocation, Bounds, BuildableClass)) {
    return false;
  }

  const FVector Closest = Bounds.GetClosestPointTo(AnchorLocation);
  if (FVector::DistSquared2D(AnchorLocation, Closest) >
      StructuralPowerConstants::MaxStructuralAttachDistCmSq) {
    return false;
  }

  return Bounds.ComputeSquaredDistanceToPoint(AnchorLocation) <=
         StructuralPowerConstants::MaxStructuralAttachDistCmSq;
}

static FBox ExpandBoundsForClass(const FBox& Bounds, TSubclassOf<AFGBuildable> BuildableClass) {
  if (!Bounds.IsValid) {
    return Bounds;
  }

  FBox Expanded = Bounds.ExpandBy(StructuralPowerConstants::StructuralConnectivityGapCm);
  const AFGBuildable* CDO =
      BuildableClass ? BuildableClass->GetDefaultObject<AFGBuildable>() : nullptr;
  if (!CDO) {
    return Expanded;
  }

  if (IsPreferredWallClass(CDO)) {
    Expanded.Min.Z -= 100.0f;
    Expanded.Max.Z += 100.0f;
  } else if (IsPreferredFoundationClass(CDO)) {
    Expanded.Max.Z += StructuralPowerConstants::GroundPoleFoundationVerticalReachCm;
  } else if (IsFactoryBuildingClass(CDO)) {
    Expanded.Max.Z += StructuralPowerConstants::GroundPoleFoundationVerticalReachCm;
  }

  return Expanded;
}
} // namespace

namespace FStructuralOutletParentHeuristics {
bool IsFactoryBuildingClass(const AFGBuildable* Buildable) {
  return ::IsFactoryBuildingClass(Buildable);
}

FBox ExpandFactoryBuildingBounds(const FBox& Bounds) {
  if (!Bounds.IsValid) {
    return Bounds;
  }

  FBox Expanded = Bounds.ExpandBy(StructuralPowerConstants::StructuralConnectivityGapCm);
  Expanded.Max.Z += StructuralPowerConstants::GroundPoleFoundationVerticalReachCm;
  return Expanded;
}

bool IsEndpointOnFactoryBuildingTop(const FVector& AnchorLocation, const FBox& Bounds) {
  if (!Bounds.IsValid) {
    return false;
  }

  const float Gap = StructuralPowerConstants::StructuralConnectivityGapCm;
  if (AnchorLocation.X < (Bounds.Min.X - Gap) || AnchorLocation.X > (Bounds.Max.X + Gap) ||
      AnchorLocation.Y < (Bounds.Min.Y - Gap) || AnchorLocation.Y > (Bounds.Max.Y + Gap)) {
    return false;
  }

  const float TopZ = Bounds.Max.Z;
  return AnchorLocation.Z >= (TopZ - Gap - 50.0f) &&
         AnchorLocation.Z <= (TopZ + StructuralPowerConstants::GroundPoleFoundationVerticalReachCm);
}

bool IsPreferredWallClass(const AFGBuildable* Buildable) {
  return ::IsPreferredWallClass(Buildable);
}

bool IsPreferredFoundationClass(const AFGBuildable* Buildable) {
  return ::IsPreferredFoundationClass(Buildable);
}

bool PrefersFoundationAnchor(const AFGBuildable* Outlet) {
  if (IsWallMountedSwitch(Outlet)) {
    return false;
  }

  if (IsValid(Outlet) && Outlet->IsA<AFGBuildableCircuitSwitch>()) {
    return true;
  }

  if (FStructuralEligibilityRules::PrefersFoundationMount(Outlet)) {
    return true;
  }

  const AFGBuildablePowerPole* Pole = Cast<AFGBuildablePowerPole>(Outlet);
  if (!Pole) {
    return false;
  }

  switch (Pole->GetPowerPoleType()) {
  case EPowerPoleType::PPT_POLE:
  case EPowerPoleType::PPT_TOWER:
    return true;
  default:
    return false;
  }
}

bool IsWallOutlet(const AFGBuildable* Outlet) {
  if (IsWallMountedSwitch(Outlet)) {
    return true;
  }

  const AFGBuildablePowerPole* Pole = Cast<AFGBuildablePowerPole>(Outlet);
  if (!Pole) {
    return false;
  }

  switch (Pole->GetPowerPoleType()) {
  case EPowerPoleType::PPT_WALL:
  case EPowerPoleType::PPT_WALL_DOUBLE:
    return true;
  default:
    return false;
  }
}

FVector GetOutletAnchorLocation(const AFGBuildable* Outlet) {
  if (!IsValid(Outlet)) {
    return FVector::ZeroVector;
  }

  if (!PrefersFoundationAnchor(Outlet)) {
    return Outlet->GetActorLocation();
  }

  FVector Origin;
  FVector Extent;
  Outlet->GetActorBounds(false, Origin, Extent);
  FVector Foot = Outlet->GetActorLocation();
  Foot.Z = Origin.Z - Extent.Z;
  return Foot;
}

bool IsGroundPoleOnFoundationFootprint(const FVector& AnchorLocation, const FBox& Bounds,
                                       TSubclassOf<AFGBuildable> BuildableClass) {
  return ::IsGroundPoleOnFoundationFootprint(AnchorLocation, Bounds, BuildableClass);
}

bool IsConfirmedStructuralAttach(const AFGBuildable* Outlet, const FBox& ParentBounds,
                                 TSubclassOf<AFGBuildable> ParentClass) {
  if (!IsValid(Outlet) || !ParentBounds.IsValid) {
    return false;
  }

  const FVector AnchorLocation = GetOutletAnchorLocation(Outlet);
  const AFGBuildable* ParentCdo =
      ParentClass ? ParentClass->GetDefaultObject<AFGBuildable>() : nullptr;

  if (PrefersFoundationAnchor(Outlet) && ParentCdo && IsPreferredFoundationClass(ParentCdo)) {
    return IsGroundPoleOnFoundationFootprint(AnchorLocation, ParentBounds, ParentClass);
  }

  if (ParentCdo && IsFactoryBuildingClass(ParentCdo)) {
    return IsEndpointOnFactoryBuildingTop(AnchorLocation, ParentBounds);
  }

  FBox Expanded = ParentBounds.ExpandBy(StructuralPowerConstants::StructuralConnectivityGapCm);
  if (ParentCdo) {
    if (IsPreferredWallClass(ParentCdo)) {
      Expanded.Min.Z -= 100.0f;
      Expanded.Max.Z += 100.0f;
    } else if (IsPreferredFoundationClass(ParentCdo)) {
      Expanded.Max.Z += StructuralPowerConstants::GroundPoleFoundationVerticalReachCm;
    }
  }

  return Expanded.IsValid && Expanded.IsInsideOrOn(AnchorLocation);
}

bool IsOutletNearBounds(const FBox& Bounds, TSubclassOf<AFGBuildable> BuildableClass,
                        const AFGBuildable* Outlet) {
  if (!IsValid(Outlet)) {
    return false;
  }

  const AFGBuildable* CDO =
      BuildableClass ? BuildableClass->GetDefaultObject<AFGBuildable>() : nullptr;
  const FVector AnchorLocation = GetOutletAnchorLocation(Outlet);
  if (PrefersFoundationAnchor(Outlet) && CDO && IsPreferredFoundationClass(CDO)) {
    return IsGroundPoleParentCandidate(AnchorLocation, Bounds, BuildableClass);
  }

  if (CDO && IsFactoryBuildingClass(CDO)) {
    const FBox Expanded = ExpandFactoryBuildingBounds(Bounds);
    if (Expanded.IsValid && Expanded.IsInsideOrOn(AnchorLocation)) {
      return true;
    }

    return IsEndpointOnFactoryBuildingTop(AnchorLocation, Bounds);
  }

  const FBox Expanded = ExpandBoundsForClass(Bounds, BuildableClass);
  if (Expanded.IsValid && Expanded.IsInsideOrOn(AnchorLocation)) {
    return true;
  }

  return Bounds.IsValid && Bounds.ComputeSquaredDistanceToPoint(AnchorLocation) <=
                               StructuralPowerConstants::MaxStructuralAttachDistCmSq;
}

float ScoreParentCandidate(const FVector& AnchorLocation, const FBox& Bounds,
                           TSubclassOf<AFGBuildable> BuildableClass, const AFGBuildable* Outlet) {
  if (!Bounds.IsValid) {
    return TNumericLimits<float>::Max();
  }

  const FBox Expanded = ExpandBoundsForClass(Bounds, BuildableClass);
  const AFGBuildable* CDO =
      BuildableClass ? BuildableClass->GetDefaultObject<AFGBuildable>() : nullptr;
  const bool bOnFootprint =
      PrefersFoundationAnchor(Outlet) && CDO && IsPreferredFoundationClass(CDO) &&
      IsGroundPoleOnFoundationFootprint(GetOutletAnchorLocation(Outlet), Bounds, BuildableClass);
  const bool bNearFoundation =
      PrefersFoundationAnchor(Outlet) && CDO && IsPreferredFoundationClass(CDO) &&
      IsGroundPoleParentCandidate(GetOutletAnchorLocation(Outlet), Bounds, BuildableClass);
  const bool bInsideFace =
      (Expanded.IsValid && Expanded.IsInsideOrOn(AnchorLocation)) || bOnFootprint;
  const float DistSq = Bounds.ComputeSquaredDistanceToPoint(AnchorLocation);

  if (IsWallOutlet(Outlet) && CDO) {
    if (IsPreferredWallClass(CDO)) {
      return bInsideFace ? DistSq * 0.001f : DistSq;
    }

    if (IsPreferredFoundationClass(CDO)) {
      return 1.0e8f + DistSq;
    }
  }

  if (PrefersFoundationAnchor(Outlet) && CDO && IsPreferredFoundationClass(CDO)) {
    if (!bNearFoundation) {
      return TNumericLimits<float>::Max();
    }

    return bOnFootprint ? DistSq * 0.001f : DistSq;
  }

  if (PrefersFoundationAnchor(Outlet) && CDO && IsPreferredWallClass(CDO)) {
    return 1.0e6f + DistSq;
  }

  return bInsideFace ? DistSq * 0.01f : DistSq;
}
}
