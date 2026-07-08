// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Graph/FStructuralHostAttachAdapter.h"

#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableFactoryBuilding.h"
#include "Graph/FStructuralOutletParentHeuristics.h"
#include "Graph/FStructuralOutletParentResolver.h"
#include "Rules/FStructuralEligibilityRules.h"
#include "StructuralPowerConstants.h"

namespace
{
static EStructuralHostKind ClassifyHostClass(const AFGBuildable* Host)
{
	if (!IsValid(Host))
	{
		return EStructuralHostKind::Unknown;
	}

	if (FStructuralOutletParentHeuristics::IsPreferredFoundationClass(Host))
	{
		return EStructuralHostKind::Foundation;
	}

	if (FStructuralOutletParentHeuristics::IsPreferredWallClass(Host))
	{
		return EStructuralHostKind::Wall;
	}

	if (Host->IsA<AFGBuildableFactoryBuilding>())
	{
		return EStructuralHostKind::FactoryBuilding;
	}

	if (FStructuralEligibilityRules::IsBusMember(Host))
	{
		return EStructuralHostKind::GenericBusMember;
	}

	return EStructuralHostKind::Unknown;
}
} // namespace

EStructuralHostKind FStructuralHostAttachAdapter::ClassifyHost(
	const FStructuralWallAnchor& Anchor)
{
	if (IsValid(Anchor.Actor))
	{
		return ClassifyHostClass(Anchor.Actor);
	}

	if (Anchor.Lightweight.IsValid())
	{
		return EStructuralHostKind::Lightweight;
	}

	return EStructuralHostKind::Unknown;
}

bool FStructuralHostAttachAdapter::ConfirmSiteAttach(
	const FStructuralOutletParentResolveResult& ParentResolve,
	AFGBuildable* Endpoint)
{
	if (!ParentResolve.Anchor.IsValid() || !IsValid(Endpoint))
	{
		return false;
	}

	switch (ParentResolve.Method)
	{
	case EStructuralOutletParentMethod::ActorParent:
	case EStructuralOutletParentMethod::LightweightFaceAttach:
	case EStructuralOutletParentMethod::LightweightIndex:
		return true;
	case EStructuralOutletParentMethod::StructureGraph:
	{
		const EStructuralHostKind HostKind = ClassifyHost(ParentResolve.Anchor);
		TSubclassOf<AFGBuildable> ParentClass;
		if (IsValid(ParentResolve.Anchor.Actor))
		{
			ParentClass = ParentResolve.Anchor.Actor->GetClass();
		}
		else if (ParentResolve.Anchor.Lightweight.IsValid())
		{
			ParentClass = ParentResolve.Anchor.Lightweight.BuildableClass;
		}

		FBox ParentBounds = FStructuralOutletParentResolver::BoundsForStructuralAnchor(
			ParentResolve.Anchor,
			Endpoint->GetWorld());
		if (HostKind == EStructuralHostKind::FactoryBuilding
			|| HostKind == EStructuralHostKind::GenericBusMember)
		{
			ParentBounds = ExpandHostBounds(ParentBounds, ParentClass, HostKind);
		}

		return FStructuralOutletParentHeuristics::IsConfirmedStructuralAttach(
			Endpoint,
			ParentBounds,
			ParentClass);
	}
	default:
		return false;
	}
}

FBox FStructuralHostAttachAdapter::ExpandHostBounds(
	const FBox& Bounds,
	TSubclassOf<AFGBuildable> HostClass,
	EStructuralHostKind HostKind)
{
	if (!Bounds.IsValid)
	{
		return Bounds;
	}

	const AFGBuildable* CDO = HostClass ? HostClass->GetDefaultObject<AFGBuildable>() : nullptr;
	if (!CDO)
	{
		return Bounds;
	}

	if (HostKind == EStructuralHostKind::Foundation
		|| FStructuralOutletParentHeuristics::IsPreferredFoundationClass(CDO))
	{
		return Bounds.ExpandBy(StructuralPowerConstants::StructuralConnectivityGapCm)
			.ExpandBy(FVector(
				0.0,
				0.0,
				StructuralPowerConstants::GroundPoleFoundationVerticalReachCm));
	}

	if (HostKind == EStructuralHostKind::Wall
		|| FStructuralOutletParentHeuristics::IsPreferredWallClass(CDO))
	{
		FBox Expanded = Bounds.ExpandBy(StructuralPowerConstants::StructuralConnectivityGapCm);
		Expanded.Min.Z -= 100.0f;
		Expanded.Max.Z += 100.0f;
		return Expanded;
	}

	if (HostKind == EStructuralHostKind::FactoryBuilding
		|| HostKind == EStructuralHostKind::GenericBusMember
		|| CDO->IsA<AFGBuildableFactoryBuilding>())
	{
		return FStructuralOutletParentHeuristics::ExpandFactoryBuildingBounds(Bounds);
	}

	return Bounds.ExpandBy(StructuralPowerConstants::StructuralConnectivityGapCm);
}
