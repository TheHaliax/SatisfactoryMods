// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Graph/FStructuralCrossSiteGraph.h"

#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Circuit/FStructuralCircuitPromotionUtil.h"
#include "FGCircuitConnectionComponent.h"
#include "Graph/FStructuralSwitchParentResolver.h"
#include "Save/AStructuralPowerGraphSubsystem.h"

void FStructuralCrossSiteGraph::Clear()
{
	SiteAdjacency.Empty();
	CachedFeedSignatures.Empty();
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

FStructuralSiteFeedSignature FStructuralCrossSiteGraph::ComputeSiteFeedSignature(
	AStructuralPowerGraphSubsystem& Graph,
	int32 Site)
{
	FStructuralSiteFeedSignature Signature;
	if (Site == INDEX_NONE)
	{
		return Signature;
	}

	Signature.bPowered = Graph.DoesComponentRootCarryPower(Site);
	if (!Signature.bPowered)
	{
		return Signature;
	}

	if (UFGCircuitConnectionComponent* Feed = Graph.GetComponentSourceConnector(Site, nullptr))
	{
		Signature.CircuitId = Feed->GetCircuitID();
		if (!FStructuralCircuitPromotionUtil::ComponentCarriesPower(Cast<UFGPowerConnectionComponent>(Feed)))
		{
			Signature.bPowered = false;
			Signature.CircuitId = INDEX_NONE;
		}
	}

	return Signature;
}

void FStructuralCrossSiteGraph::SeedFeedSignature(
	AStructuralPowerGraphSubsystem& Graph,
	int32 Site)
{
	if (Site == INDEX_NONE)
	{
		return;
	}

	CachedFeedSignatures.Add(Site, ComputeSiteFeedSignature(Graph, Site));
}

void FStructuralCrossSiteGraph::SeedFeedSignaturesForSites(
	AStructuralPowerGraphSubsystem& Graph,
	const TSet<int32>& Sites)
{
	for (int32 Site : Sites)
	{
		SeedFeedSignature(Graph, Site);
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
	TArray<int32> WiredSites;
	CollectWiredSwitchSites(Graph, Switch, OriginSite, WiredSites);

	for (int32 Site : WiredSites)
	{
		if (Site != OriginSite)
		{
			AddCoupling(OriginSite, Site);
		}
	}
}

void FStructuralCrossSiteGraph::TraceFeedAffected(
	AStructuralPowerGraphSubsystem& Graph,
	AFGBuildableCircuitSwitch* TriggerSwitch,
	int32 OriginSite,
	TArray<int32>& OutAffectedSites)
{
	OutAffectedSites.Reset();
	if (OriginSite == INDEX_NONE)
	{
		return;
	}

	TSet<int32> Visited;
	TArray<int32> Queue;

	auto EnqueueSite = [&](int32 Site)
	{
		if (Site != INDEX_NONE && !Visited.Contains(Site))
		{
			Queue.AddUnique(Site);
		}
	};

	EnqueueSite(OriginSite);
	if (IsValid(TriggerSwitch))
	{
		TArray<int32> WiredSites;
		CollectWiredSwitchSites(Graph, TriggerSwitch, OriginSite, WiredSites);
		for (int32 Site : WiredSites)
		{
			EnqueueSite(Site);
		}
	}

	int32 QueueHead = 0;
	while (QueueHead < Queue.Num())
	{
		const int32 Site = Queue[QueueHead++];
		if (Visited.Contains(Site))
		{
			continue;
		}
		Visited.Add(Site);

		const FStructuralSiteFeedSignature NewSignature = ComputeSiteFeedSignature(Graph, Site);
		if (const FStructuralSiteFeedSignature* Cached = CachedFeedSignatures.Find(Site))
		{
			if (*Cached == NewSignature)
			{
				continue;
			}
		}

		CachedFeedSignatures.Add(Site, NewSignature);
		OutAffectedSites.Add(Site);

		TArray<int32> Neighbors;
		GetCoupledSites(Site, Neighbors);
		for (int32 Neighbor : Neighbors)
		{
			EnqueueSite(Neighbor);
		}
	}

	OutAffectedSites.Sort();
}
