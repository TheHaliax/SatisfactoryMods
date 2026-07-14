// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Core/EAttachContext.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Graph/FStructuralSiteContext.h"

class AFGBuildable;
class AFGBuildableCircuitSwitch;
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

struct FStructuralSwitchMembershipParams
{
	bool bAdvancedWork = false;
	bool bKeyedSubnet = false;
	bool bSwitchOn = false;
	EAttachContext AttachContext = EAttachContext::RuntimePlace;
};

struct FStructuralSwitchMembershipResult
{
	FStructuralSiteContext Site;
	UFGStructuralPowerConnectionComponent* OutletBus = nullptr;
	bool bDone = false;
	bool bApplyBridgeStrategy = false;
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

	/** Switch site bus/mesh template — kind bridge strategy stays outside. */
	static FStructuralSwitchMembershipResult IntegrateSwitchOnPlace(
		FStructuralGraphSession& Session,
		AFGBuildableCircuitSwitch* Switch,
		const FStructuralSwitchMembershipParams& Params);

	static bool ReaffirmMountParent(
		FStructuralGraphSession& Session,
		AFGBuildable* Host,
		FTrackedEndpoint& Tracked,
		bool bUsePoleRootResolver = false);

	static int32 SiteRootFromMount(FStructuralGraphSession& Session, const FStructuralNodeId& MountParentId);
};
