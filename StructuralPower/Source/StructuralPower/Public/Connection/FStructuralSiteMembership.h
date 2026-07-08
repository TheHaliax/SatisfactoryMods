// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Graph/FStructuralSiteContext.h"

class AFGBuildable;
class AFGBuildablePowerPole;
class AStructuralPowerGraphSubsystem;
class UFGStructuralPowerConnectionComponent;

struct FStructuralSiteMembershipParams
{
	bool bLinkVisibleConnections = false;
	bool bBridgePeersOnly = true;
	bool bStripSwitchVanillaPortLinks = false;
	/** When true, caller updates EndpointIndex incrementally — avoids full rebuild hitch on runtime place. */
	bool bSkipEndpointIndexDirty = false;
};

class STRUCTURALPOWER_API FStructuralSiteMembership
{
public:
	static bool ResolveSiteContext(
		AStructuralPowerGraphSubsystem& Graph,
		AFGBuildable* Endpoint,
		FStructuralSiteContext& OutSite,
		bool bUsePoleRootResolver = false);

	static void IntegrateOnPlace(
		AStructuralPowerGraphSubsystem& Graph,
		AFGBuildable* Host,
		UFGStructuralPowerConnectionComponent* OutletBus,
		const FStructuralNodeId& EndpointId,
		EStructuralEndpointKind Kind,
		FTrackedEndpoint& Tracked,
		const FStructuralSiteContext& Site,
		const FStructuralSiteMembershipParams& Params = FStructuralSiteMembershipParams());
};
