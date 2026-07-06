// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class AFGBuildableCircuitSwitch;
class AStructuralPowerGraphSubsystem;

/** Site adjacency for cross-site structural power (Phase C3 stub; feed trace in C4). */
class STRUCTURALPOWER_API FStructuralCrossSiteGraph
{
public:
	void Clear();

	/** Phase C3 — legacy 1-hop wired-switch discovery; records site couplings. */
	void RefreshCouplingsFromWiredSwitch(
		AStructuralPowerGraphSubsystem& Graph,
		AFGBuildableCircuitSwitch* Switch,
		int32 OriginSite);

	/** Phase C3 stub — returns feed-affected sites (same set as refresh until C4 signatures). */
	void TraceFeedAffectedFromWiredSwitch(
		AStructuralPowerGraphSubsystem& Graph,
		AFGBuildableCircuitSwitch* Switch,
		int32 OriginSite,
		TArray<int32>& OutAffectedSites);

	bool AreSitesCoupled(int32 SiteA, int32 SiteB) const;
	void GetCoupledSites(int32 Site, TArray<int32>& OutSites) const;

private:
	void AddCoupling(int32 SiteA, int32 SiteB);
	void CollectWiredSwitchSites(
		AStructuralPowerGraphSubsystem& Graph,
		AFGBuildableCircuitSwitch* Switch,
		int32 LocalRoot,
		TArray<int32>& OutSites);

	TMap<int32, TSet<int32>> SiteAdjacency;
};
