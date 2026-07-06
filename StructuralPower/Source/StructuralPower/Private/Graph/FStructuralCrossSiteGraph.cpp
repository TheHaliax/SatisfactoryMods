// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Graph/FStructuralCrossSiteGraph.h"

#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Graph/FStructuralSwitchParentResolver.h"
#include "Save/AStructuralPowerGraphSubsystem.h"

void FStructuralCrossSiteGraph::Clear()
{
	SiteAdjacency.Empty();
}

void FStructuralCrossSiteGraph::AddCoupling(int32 SiteA, int32 SiteB)
{
	if (SiteA == INDEX_NONE || SiteB == INDEX_NONE || SiteA == SiteB)
	{
		return;
	}

	SiteAdjacency.FindOrAdd(SiteA).Add(SiteB);
	SiteAdjacency.FindOrAdd(SiteB).Add(SiteA);
}

bool FStructuralCrossSiteGraph::AreSitesCoupled(int32 SiteA, int32 SiteB) const
{
	if (SiteA == INDEX_NONE || SiteB == INDEX_NONE || SiteA == SiteB)
	{
		return SiteA == SiteB && SiteA != INDEX_NONE;
	}

	if (const TSet<int32>* Neighbors = SiteAdjacency.Find(SiteA))
	{
		return Neighbors->Contains(SiteB);
	}

	return false;
}

void FStructuralCrossSiteGraph::GetCoupledSites(int32 Site, TArray<int32>& OutSites) const
{
	OutSites.Reset();
	if (Site == INDEX_NONE)
	{
		return;
	}

	if (const TSet<int32>* Neighbors = SiteAdjacency.Find(Site))
	{
		OutSites = Neighbors->Array();
		OutSites.Sort();
	}
}

void FStructuralCrossSiteGraph::CollectWiredSwitchSites(
	AStructuralPowerGraphSubsystem& Graph,
	AFGBuildableCircuitSwitch* Switch,
	int32 LocalRoot,
	TArray<int32>& OutSites)
{
	TSet<int32> Roots;
	auto ConsiderRoot = [&](int32 Root)
	{
		if (Root != INDEX_NONE)
		{
			Roots.Add(Root);
		}
	};

	ConsiderRoot(LocalRoot);

	if (!IsValid(Switch))
	{
		OutSites = Roots.Array();
		return;
	}

	UWorld* World = Graph.GetWorld();
	if (!IsValid(World))
	{
		OutSites = Roots.Array();
		return;
	}

	const FStructuralOutletParentResolveParams ParentParams = Graph.MakeOutletParentResolveParams();
	FStructuralSwitchParentResolver::ForEachWiredStructureSideAnchor(
		Switch,
		World,
		Graph.LightweightIndex,
		&ParentParams,
		[&](const FStructuralWallAnchor& Anchor)
		{
			FStructuralNodeId ParentId;
			if (Graph.EnsureParentRegisteredInGraph(Anchor, ParentId))
			{
				ConsiderRoot(Graph.StructureGraph.FindRoot(ParentId));
			}
		});

	OutSites = Roots.Array();
}

void FStructuralCrossSiteGraph::RefreshCouplingsFromWiredSwitch(
	AStructuralPowerGraphSubsystem& Graph,
	AFGBuildableCircuitSwitch* Switch,
	int32 OriginSite)
{
	TArray<int32> AffectedSites;
	CollectWiredSwitchSites(Graph, Switch, OriginSite, AffectedSites);

	for (int32 Site : AffectedSites)
	{
		if (Site != OriginSite)
		{
			AddCoupling(OriginSite, Site);
		}
	}
}

void FStructuralCrossSiteGraph::TraceFeedAffectedFromWiredSwitch(
	AStructuralPowerGraphSubsystem& Graph,
	AFGBuildableCircuitSwitch* Switch,
	int32 OriginSite,
	TArray<int32>& OutAffectedSites)
{
	CollectWiredSwitchSites(Graph, Switch, OriginSite, OutAffectedSites);
}
