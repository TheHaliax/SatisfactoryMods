// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Graph/FStructuralSiteContext.h"

class AFGBuildable;
class AFGBuildablePowerPole;
class FStructuralGraphSession;
class UFGStructuralPowerConnectionComponent;

struct FStructuralSiteMembershipParams
{
	bool bLinkVisibleConnections = false;
	bool bBridgePeersOnly = true;
	bool bStripSwitchVanillaPortLinks = false;
	bool bMeshOnlyLinks = false;
	bool bSkipEndpointIndexDirty = false;
};

class STRUCTURALPOWER_API FStructuralSiteMembership
{
public:
	static bool ResolveSiteContext(
		FStructuralGraphSession& Session,
		AFGBuildable* Endpoint,
		FStructuralSiteContext& OutSite,
		bool bUsePoleRootResolver = false);

	static void RegisterOnBulkLoad(
		FStructuralGraphSession& Session,
		AFGBuildable* Host,
		EStructuralEndpointKind Kind,
		FTrackedEndpoint& Tracked,
		const FStructuralSiteContext& Site);

	static void IntegrateOnPlace(
		FStructuralGraphSession& Session,
		AFGBuildable* Host,
		UFGStructuralPowerConnectionComponent* OutletBus,
		const FStructuralNodeId& EndpointId,
		EStructuralEndpointKind Kind,
		FTrackedEndpoint& Tracked,
		const FStructuralSiteContext& Site,
		const FStructuralSiteMembershipParams& Params = FStructuralSiteMembershipParams());
};
