// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Graph/FStructuralSiteFeedSignature.h"

class AFGBuildableCircuitSwitch;
class FStructuralGraphSession;

class STRUCTURALPOWER_API FStructuralCrossSiteGraph
{
public:
	void Clear();

	void RefreshCouplingsFromWiredSwitch(
		FStructuralGraphSession& Session,
		AFGBuildableCircuitSwitch* Switch,
		int32 OriginSite);

	void TraceFeedAffected(
		FStructuralGraphSession& Session,
		AFGBuildableCircuitSwitch* TriggerSwitch,
		int32 OriginSite,
		TArray<int32>& OutAffectedSites);

	void GatherSwitchSiteRoots(
		FStructuralGraphSession& Session,
		AFGBuildableCircuitSwitch* Switch,
		int32 LocalRoot,
		TArray<int32>& OutSites);

	void SeedFeedSignature(FStructuralGraphSession& Session, int32 Site);
	void SeedFeedSignaturesForSites(
		FStructuralGraphSession& Session,
		const TSet<int32>& Sites);

	bool AreSitesCoupled(int32 SiteA, int32 SiteB) const;
	void GetCoupledSites(int32 Site, TArray<int32>& OutSites) const;

private:
	void AddCoupling(int32 SiteA, int32 SiteB);
	void CollectWiredSwitchSites(
		FStructuralGraphSession& Session,
		AFGBuildableCircuitSwitch* Switch,
		int32 LocalRoot,
		TArray<int32>& OutSites);

	static FStructuralSiteFeedSignature ComputeSiteFeedSignature(
		FStructuralGraphSession& Session,
		int32 Site);

	TMap<int32, TSet<int32>> SiteAdjacency;
};
