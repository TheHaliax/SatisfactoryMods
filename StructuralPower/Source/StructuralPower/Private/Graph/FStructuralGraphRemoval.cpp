// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Graph/FStructuralGraphRemoval.h"

#include "Core/FStructuralGraphSession.h"

#include "Buildables/FGBuildable.h"
#include "Components/UFGStructuralPowerConnectionComponent.h"
#include "FGCircuitConnectionComponent.h"
#include "FGPowerConnectionComponent.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Lightweight/FStructuralLightweightIndex.h"
#include "Lightweight/FStructuralLightweightTypes.h"
#include "Processors/FStructuralEndpointDispatcher.h"
#include "Save/FStructuralPlacementQueue.h"

void FStructuralGraphRemoval::Bind(FStructuralGraphSession* InSession)
{
	Session = InSession;
}

void FStructuralGraphRemoval::OnBuildableRemoved(AFGBuildable* Buildable)
{
	if (!Session || !IsValid(Buildable))
	{
		return;
	}

	const FStructuralNodeId NodeId = Session->MakeNodeId(Buildable);
	int32 GangRoot = INDEX_NONE;
	if (const FTrackedEndpoint* TrackedBefore = Session->TrackedEndpoints().Find(NodeId))
	{
		GangRoot = Session->StructureGraph().FindRoot(TrackedBefore->MountParentId);
	}

	Session->ControlIdGangIndex().RemoveNode(NodeId);
	Session->UnregisterBuildableActor(NodeId);

	if (FTrackedEndpoint* Tracked = Session->TrackedEndpoints().Find(NodeId))
	{
		const int32 OldRoot = Session->StructureGraph().FindRoot(Tracked->MountParentId);

		if (AFGBuildable* Host = Tracked->Actor.Get())
		{
			FStructuralEndpointDispatcher::DispatchTeardown(*Session, Host);

			if (Tracked->Kind != EStructuralEndpointKind::Switch
				&& Tracked->Kind != EStructuralEndpointKind::Light
				&& Tracked->Kind != EStructuralEndpointKind::Panel)
			{
				if (UFGStructuralPowerConnectionComponent* Bus = Session->FindBusConnector(Host))
				{
					TArray<UFGCircuitConnectionComponent*> HiddenLinks;
					Bus->GetHiddenConnections(HiddenLinks);
					for (UFGCircuitConnectionComponent* OtherRaw : HiddenLinks)
					{
						if (UFGPowerConnectionComponent* Other = Cast<UFGPowerConnectionComponent>(OtherRaw))
						{
							Bus->RemoveHiddenConnection(Other);
						}
					}
				}
			}
		}

		(void)OldRoot;
		Session->TrackedEndpoints().Remove(NodeId);
		Session->MarkBridgeEndpointRootIndexDirty();
	}
	else if (Session->StructureGraph().IsTracked(NodeId))
	{
		TArray<int32> AffectedRoots;
		Session->StructureGraph().RemoveNode(NodeId, AffectedRoots);
	}

	Session->Placement().RemoveBuildable(Buildable);

	if (GangRoot != INDEX_NONE)
	{
		Session->RebuildControlIdGangsForRoot(GangRoot);
	}
}

void FStructuralGraphRemoval::OnLightweightRemoved(const FStructuralLightweightKey& Key)
{
	if (!Session || !Key.IsValid())
	{
		return;
	}

	const FStructuralNodeId NodeId = FStructuralLightweightIndex::MakeNodeId(Key);
	if (Session->StructureGraph().IsTracked(NodeId))
	{
		TArray<int32> AffectedRoots;
		Session->StructureGraph().RemoveNode(NodeId, AffectedRoots);
	}

	Session->LightweightIndex().UnregisterMember(Key);
	Session->Placement().RemoveLightweight(Key);
}
