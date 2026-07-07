// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Graph/FStructuralBridgeRootIndex.h"

#include "Save/AStructuralPowerGraphSubsystem.h"

#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "Graph/FStructuralAdjacencyHeuristics.h"
#include "Graph/FStructuralAttachmentResolver.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Rules/FStructuralEligibilityRules.h"
#include "StructuralPowerLog.h"

void FStructuralBridgeRootIndex::Bind(AStructuralPowerGraphSubsystem* InSubsystem)
{
	Subsystem = InSubsystem;
}

void FStructuralBridgeRootIndex::MarkBridgeEndpointRootIndexDirty()
{
	Subsystem->bBridgeEndpointRootIndexDirty = true;
	Subsystem->SourceConnectorByRoot.Empty();
}

void FStructuralBridgeRootIndex::RefreshBridgeEndpointRootIndex()
{
	if (!Subsystem->bBridgeEndpointRootIndexDirty)
	{
		return;
	}

	Subsystem->EndpointIndex.RebuildFrom(Subsystem->TrackedEndpoints, Subsystem->StructureGraph);
	Subsystem->bBridgeEndpointRootIndexDirty = false;
}

void FStructuralBridgeRootIndex::AddEndpointToRootIndex(
	int32 Root,
	EStructuralEndpointKind Kind,
	const FStructuralNodeId& EndpointId)
{
	if (Root == INDEX_NONE || !EndpointId.IsValid())
	{
		return;
	}

	if (Subsystem->bBridgeEndpointRootIndexDirty)
	{
		return;
	}

	Subsystem->EndpointIndex.Add(Root, Kind, EndpointId);
}

int32 FStructuralBridgeRootIndex::FindRootForTrackedEndpoint(
	const FTrackedEndpoint& Tracked) const
{
	if (!Tracked.ParentId.IsValid())
	{
		return INDEX_NONE;
	}

	return Subsystem->StructureGraph.FindRoot(Tracked.ParentId);
}

int32 FStructuralBridgeRootIndex::ResolveBridgeRootFromAnchor(
	AFGBuildable* Host,
	const FStructuralWallAnchor& Anchor,
	FStructuralNodeId& OutParentId,
	bool bPreferBulkResolve)
{
	if (bPreferBulkResolve)
	{
		if (AFGBuildablePowerPole* Pole = Cast<AFGBuildablePowerPole>(Host))
		{
			return ResolvePoleComponentRoot(Pole, Anchor, OutParentId);
		}

		return ResolveBridgeComponentRootBulk(Host, Anchor, OutParentId);
	}

	return ResolveEndpointComponentRoot(Host, Anchor, OutParentId);
}

int32 FStructuralBridgeRootIndex::ResolveBridgeComponentRootBulk(
	AFGBuildable* Host,
	const FStructuralWallAnchor& Anchor,
	FStructuralNodeId& OutParentId)
{
	OutParentId = FStructuralNodeId();
	if (!IsValid(Host))
	{
		return INDEX_NONE;
	}

	if (Anchor.IsValid())
	{
		const FStructuralNodeId ParentId = AStructuralPowerGraphSubsystem::MakeParentNodeId(Anchor);
		const int32 Root = Subsystem->StructureGraph.FindRoot(ParentId);
		if (Root != INDEX_NONE)
		{
			OutParentId = ParentId;
			return Root;
		}
	}

	const FBox Bounds = FStructuralAdjacencyHeuristics::GetActorAdjacencyBounds(Host);
	return Subsystem->StructureGraph.FindRootForBounds(Bounds, Host->GetClass(), &OutParentId);
}

int32 FStructuralBridgeRootIndex::ResolvePoleComponentRoot(
	AFGBuildablePowerPole* Pole,
	const FStructuralWallAnchor& Anchor,
	FStructuralNodeId& OutParentId)
{
	return ResolveBridgeComponentRootBulk(Pole, Anchor, OutParentId);
}

bool FStructuralBridgeRootIndex::EnsureParentRegisteredInGraph(
	const FStructuralWallAnchor& Anchor,
	FStructuralNodeId& OutParentId)
{
	if (!Anchor.IsValid())
	{
		return false;
	}

	const FStructuralNodeId ParentId = AStructuralPowerGraphSubsystem::MakeParentNodeId(Anchor);
	if (Subsystem->StructureGraph.FindRoot(ParentId) != INDEX_NONE)
	{
		OutParentId = ParentId;
		return true;
	}

	if (IsValid(Anchor.Actor) && FStructuralEligibilityRules::IsBusMember(Anchor.Actor))
	{
		Subsystem->ProcessStructure(Anchor.Actor);
	}
	else if (Anchor.Lightweight.IsValid())
	{
		Subsystem->ProcessLightweightStructure(Anchor.Lightweight);
	}
	else
	{
		return false;
	}

	if (Subsystem->StructureGraph.FindRoot(ParentId) != INDEX_NONE)
	{
		OutParentId = ParentId;
		UE_LOG(LogStructuralPower, Verbose,
			TEXT("[HALSP] parent ingested into structure graph actor=%s lw=%s[%d]"),
			IsValid(Anchor.Actor) ? *Anchor.Actor->GetName() : TEXT("null"),
			Anchor.Lightweight.IsValid() ? *Anchor.Lightweight.BuildableClass->GetName() : TEXT("null"),
			Anchor.Lightweight.IsValid() ? Anchor.Lightweight.Index : INDEX_NONE);
		return true;
	}

	return false;
}

int32 FStructuralBridgeRootIndex::ResolveEndpointComponentRoot(
	AFGBuildable* Endpoint,
	const FStructuralWallAnchor& Anchor,
	FStructuralNodeId& OutParentId)
{
	if (EnsureParentRegisteredInGraph(Anchor, OutParentId))
	{
		return Subsystem->StructureGraph.FindRoot(OutParentId);
	}

	return FStructuralAttachmentResolver::ResolveComponentRootForBuildable(
		Endpoint,
		Subsystem->StructureGraph,
		Subsystem->LightweightIndex,
		Subsystem->GetWorld(),
		OutParentId);
}

int32 FStructuralBridgeRootIndex::ResolveBridgeHostComponentRoot(
	AFGBuildable* Host,
	FStructuralNodeId* OutParentId)
{
	if (!IsValid(Host))
	{
		if (OutParentId)
		{
			*OutParentId = FStructuralNodeId();
		}
		return INDEX_NONE;
	}

	FStructuralNodeId ParentId;
	const FStructuralWallAnchor Anchor = Subsystem->ResolveOutletAnchor(Host);
	const int32 Root = ResolveEndpointComponentRoot(Host, Anchor, ParentId);
	if (OutParentId)
	{
		*OutParentId = ParentId;
	}

	if (ParentId.IsValid())
	{
		if (FTrackedEndpoint* Tracked = Subsystem->TrackedEndpoints.Find(AStructuralPowerGraphSubsystem::MakeNodeId(Host)))
		{
			Tracked->ParentId = ParentId;
		}
	}

	return Root;
}

int32 FStructuralBridgeRootIndex::GetEndpointComponentRoot(AFGBuildable* Endpoint)
{
	if (!IsValid(Endpoint))
	{
		return INDEX_NONE;
	}

	const FStructuralWallAnchor Anchor = Subsystem->ResolveOutletAnchor(Endpoint);
	FStructuralNodeId ParentId;
	return ResolveEndpointComponentRoot(Endpoint, Anchor, ParentId);
}
