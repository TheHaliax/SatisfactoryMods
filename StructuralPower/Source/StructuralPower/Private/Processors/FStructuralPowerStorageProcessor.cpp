// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Processors/FStructuralPowerStorageProcessor.h"

#include "Attach/FStructuralBridgeAttach.h"
#include "Buildables/FGBuildablePowerStorage.h"
#include "Circuit/FStructuralCircuitPromotionUtil.h"
#include "Components/UFGStructuralPowerConnectionComponent.h"
#include "Connection/FStructuralSiteMembership.h"
#include "Core/EStructuralPowerRole.h"
#include "Core/EStructuralPowerScope.h"
#include "Core/FStructuralGraphSession.h"
#include "Core/FStructuralPowerContext.h"
#include "Diagnostics/FStructuralPowerTrace.h"
#include "FGCircuitConnectionComponent.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Network/UStructuralPowerStorageListener.h"
#include "Save/AStructuralPowerGraphSubsystem.h"
#include "StructuralPowerLog.h"

namespace
{
void EnsureStorageListener(
	AStructuralPowerGraphSubsystem& Graph,
	AFGBuildablePowerStorage* Storage)
{
	if (!IsValid(Storage))
	{
		return;
	}

	TInlineComponentArray<UStructuralPowerStorageListener*> Listeners;
	Storage->GetComponents(Listeners);
	if (Listeners.Num() > 0)
	{
		return;
	}

	UStructuralPowerStorageListener* Listener =
		NewObject<UStructuralPowerStorageListener>(Storage, NAME_None, RF_Transient);
	if (!Listener)
	{
		return;
	}

	Storage->AddInstanceComponent(Listener);
	Listener->RegisterComponent();
	Listener->BindSubsystem(&Graph, Storage);
}
} // namespace

void FStructuralPowerStorageProcessor::TearDown(
	FStructuralPowerContext& Ctx,
	AFGBuildablePowerStorage* Storage)
{
	if (!IsValid(Storage))
	{
		return;
	}

	UFGStructuralPowerConnectionComponent* Bus = Ctx.Session().FindBusConnector(Storage);
	if (!IsValid(Bus))
	{
		return;
	}

	TArray<UFGCircuitConnectionComponent*> HiddenLinks;
	Bus->GetHiddenConnections(HiddenLinks);
	for (UFGCircuitConnectionComponent* OtherRaw : HiddenLinks)
	{
		Bus->RemoveHiddenConnection(OtherRaw);
		if (IsValid(OtherRaw))
		{
			OtherRaw->RemoveHiddenConnection(Bus);
		}
	}
}

void FStructuralPowerStorageProcessor::Process(
	FStructuralPowerContext& Ctx,
	AFGBuildablePowerStorage* Storage)
{
	if (!IsValid(Storage))
	{
		return;
	}

	FStructuralGraphSession& Session = Ctx.Session();
	const bool bBulk = Session.IsBulkLoadDrainActive();
	const FStructuralNodeId StorageId = Session.MakeNodeId(Storage);

	if (!bBulk)
	{
		if (FStructuralBridgeAttach::HasPlacementMembership(Session,
				Storage,
				EStructuralEndpointKind::Storage))
		{
			OnWireDelta(Ctx, Storage);
			return;
		}
	}

	FStructuralBridgeAttachRequest Request;
	Request.Host = Storage;
	Request.Kind = EStructuralEndpointKind::Storage;
	Request.bUsePoleRootResolver = false;

	const FStructuralBridgeAttachOutcome Outcome = FStructuralBridgeAttach::AttachOnPlace(Ctx, Request);
	if (!Outcome.OutletBus && !(bBulk && Outcome.bAttached))
	{
		if (!bBulk)
		{
			FStructuralPowerTrace::LogPlacementSkip(
				Storage,
				TEXT("storage_bus_create_failed"),
				ELogVerbosity::Warning);
		}
		return;
	}

	const FStructuralSiteContext& Site = Outcome.Site;

	if (bBulk && Site.SiteRoot != INDEX_NONE && Site.MountParentId.IsValid())
	{
		Session.AddEndpointToRootIndex(Site.SiteRoot, EStructuralEndpointKind::Storage, StorageId);
	}
	else if (!bBulk)
	{
		Session.MarkBridgeEndpointRootIndexDirty();
	}

	EnsureStorageListener(Session.Owner(), Storage);

	if (bBulk)
	{
		UE_LOG(LogStructuralPower, Verbose,
			TEXT("[HALSP] storage %s kind=%s scope=%s site=%d role=%s root=%d parentValid=%d"
				" busCircuit=%d powered=%d"),
			*Storage->GetName(),
			StructuralEndpointKindToString(EStructuralEndpointKind::Storage),
			StructuralPowerScopeToString(EStructuralPowerScope::Site),
			Site.SiteRoot,
			StructuralPowerRoleToString(EStructuralPowerRole::Host),
			Site.SiteRoot,
			Site.bAnchored ? 1 : 0,
			-1,
			0);
	}
	else
	{
		UE_LOG(LogStructuralPower, Log,
			TEXT("[HALSP] storage %s kind=%s scope=%s site=%d role=%s root=%d parentValid=%d"
				" busCircuit=%d powered=%d"),
			*Storage->GetName(),
			StructuralEndpointKindToString(EStructuralEndpointKind::Storage),
			StructuralPowerScopeToString(EStructuralPowerScope::Site),
			Site.SiteRoot,
			StructuralPowerRoleToString(EStructuralPowerRole::Host),
			Site.SiteRoot,
			Site.bAnchored ? 1 : 0,
			Outcome.OutletBus->GetCircuitID(),
			FStructuralCircuitPromotionUtil::ComponentCarriesPower(Outcome.OutletBus) ? 1 : 0);
	}
}

void FStructuralPowerStorageProcessor::OnWireDelta(
	FStructuralPowerContext& Ctx,
	AFGBuildablePowerStorage* Storage)
{
	if (!IsValid(Storage))
	{
		return;
	}

	FStructuralGraphSession& Session = Ctx.Session();
	UFGStructuralPowerConnectionComponent* OutletBus =
		Cast<UFGStructuralPowerConnectionComponent>(Session.GetOrCreateBusConnector(Storage));
	if (!OutletBus)
	{
		return;
	}

	const FStructuralNodeId StorageId = Session.MakeNodeId(Storage);
	FTrackedEndpoint& Tracked = Session.TrackedEndpoints().FindOrAdd(StorageId);

	FStructuralSiteContext Site;
	if (!FStructuralSiteMembership::ResolveSiteContext(Session, Storage, Site))
	{
		if (Tracked.MountParentId.IsValid())
		{
			Site.MountParentId = Tracked.MountParentId;
			Site.SiteRoot = Session.FindRootForTrackedEndpoint(Tracked);
			Site.bAnchored = Site.SiteRoot != INDEX_NONE;
		}
	}

	FStructuralSiteMembershipParams Params;
	Params.bLinkVisibleConnections = true;
	Params.bBridgePeersOnly = true;
	FStructuralSiteMembership::IntegrateOnPlace(Session,
		Storage,
		OutletBus,
		StorageId,
		EStructuralEndpointKind::Storage,
		Tracked,
		Site,
		Params);

	UE_LOG(LogStructuralPower, Log,
		TEXT("[HALSP] storage wire delta %s root=%d busCircuit=%d powered=%d"),
		*Storage->GetName(),
		Site.SiteRoot,
		OutletBus->GetCircuitID(),
		FStructuralCircuitPromotionUtil::ComponentCarriesPower(OutletBus) ? 1 : 0);
}
