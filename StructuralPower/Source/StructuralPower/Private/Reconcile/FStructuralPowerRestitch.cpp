// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Reconcile/FStructuralPowerRestitch.h"

#include "Core/FStructuralGraphSession.h"
#include "Save/AStructuralPowerGraphSubsystem.h"

#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Routing/EStructuralChannel.h"
#include "Routing/FStructuralPowerRouter.h"

void FStructuralPowerRestitch::Bind(FStructuralGraphSession* InSession)
{
	Session = InSession;
}

bool FStructuralPowerRestitch::ShouldEndpointParticipateInRestitch(
	AFGBuildable* Host,
	EStructuralEndpointKind Kind)
{
	if (!IsValid(Host))
	{
		return false;
	}

	if (Kind == EStructuralEndpointKind::Pole)
	{
		const FStructuralNodeId NodeId = Session->MakeNodeId(Host);
		const FTrackedEndpoint* Tracked = Session->TrackedEndpoints().Find(NodeId);
		return Tracked != nullptr && Tracked->bStructuralPowerTransferActive;
	}

	if (Kind == EStructuralEndpointKind::Storage)
	{
		const FStructuralNodeId NodeId = Session->MakeNodeId(Host);
		const FTrackedEndpoint* Tracked = Session->TrackedEndpoints().Find(NodeId);
		return Tracked != nullptr && Tracked->bStructuralPowerTransferActive;
	}

	if (Kind == EStructuralEndpointKind::Switch)
	{
		const FStructuralNodeId NodeId = Session->MakeNodeId(Host);
		const FTrackedEndpoint* Tracked = Session->TrackedEndpoints().Find(NodeId);
		if (!Tracked || !Tracked->MountParentId.IsValid())
		{
			return false;
		}

		return Session->StructureGraph().FindRoot(Tracked->MountParentId) != INDEX_NONE;
	}

	return false;
}

bool FStructuralPowerRestitch::ShouldMeshEndpoints(
	AFGBuildable* HostA,
	AFGBuildable* HostB,
	int32 ComponentRoot) const
{
	const FStructuralChannelKey KeyA = Session->Ids().ResolveChannelKeyForBuildable(HostA);
	const FStructuralChannelKey KeyB = Session->Ids().ResolveChannelKeyForBuildable(HostB);
	const FStructuralComponentKey CompKey = Session->Ids().MakeComponentKeyForRoot(ComponentRoot);

	if (KeyA.Tag != EStructuralChannel::Switch && KeyB.Tag != EStructuralChannel::Switch)
	{
		return true;
	}

	const bool bSwitchOnA = KeyA.Tag == EStructuralChannel::Switch;
	const FName SwitchControl = bSwitchOnA ? KeyA.Control : KeyB.Control;
	const FName DeviceSource = bSwitchOnA ? KeyB.Source : KeyA.Source;
	const EStructuralChannel PeerTag = bSwitchOnA ? KeyB.Tag : KeyA.Tag;

	if (PeerTag == EStructuralChannel::Structure)
	{
		return true;
	}

	return FStructuralPowerRouter::ShouldRouteKeyedJoin(SwitchControl, DeviceSource, CompKey, CompKey);
}
