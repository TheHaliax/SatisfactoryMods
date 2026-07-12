// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Graph/FStructuralGraphBulkDrain.h"

#include "Core/EAttachContext.h"
#include "Core/FStructuralGraphSession.h"
#include "Core/FStructuralPowerContext.h"

#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Processors/FStructuralPowerSwitchProcessor.h"
#include "Processors/FStructuralSwitchBridgeStrategy.h"
#include "Processors/FStructuralPowerTransferGate.h"
#include "StructuralPowerLog.h"

namespace
{
static TSet<int32> CollectBridgeRoots(FStructuralGraphSession& Session)
{
	TSet<int32> Roots;
	for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : Session.TrackedEndpoints())
	{
		if (Pair.Value.Kind != EStructuralEndpointKind::Pole
			&& Pair.Value.Kind != EStructuralEndpointKind::Switch
			&& Pair.Value.Kind != EStructuralEndpointKind::Storage)
		{
			continue;
		}

		if (Pair.Value.Kind == EStructuralEndpointKind::Pole
			&& !Pair.Value.bStructuralPowerTransferActive)
		{
			continue;
		}

		const int32 Root = Session.StructureGraph().FindRoot(Pair.Value.ParentId);
		if (Root != INDEX_NONE)
		{
			Roots.Add(Root);
		}
	}

	return Roots;
}
} // namespace

void FStructuralGraphBulkDrain::Bind(FStructuralGraphSession* InSession)
{
	Session = InSession;
}

void FStructuralGraphBulkDrain::FinishBulkLoadDrain()
{
	if (!Session || !Session->BulkLoadDrainActive())
	{
		return;
	}

	Session->MarkBridgeEndpointRootIndexDirty();
	Session->RefreshBridgeEndpointRootIndex();

	FStructuralPowerContext SwitchCtx = Session->MakeProcessorContext(EAttachContext::BulkLoad);
	FStructuralPowerSwitchProcessor::RemeshUnmeshedBridgesAfterBulkLoad(SwitchCtx);

	const TSet<int32> Roots = CollectBridgeRoots(*Session);

	if (Roots.Num() > 0)
	{
		const TArray<int32> RootArray = Roots.Array();

		FStructuralPowerContext BulkCtx = Session->MakeProcessorContext(EAttachContext::BulkLoad);
		for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : Session->TrackedEndpoints())
		{
			if (Pair.Value.Kind != EStructuralEndpointKind::Switch)
			{
				continue;
			}

			if (AFGBuildableCircuitSwitch* Switch = Pair.Value.GetSwitch())
			{
				FStructuralSwitchBridgeStrategy::ApplyMembership(BulkCtx, Switch);
			}
		}

		for (int32 Root : RootArray)
		{
			FStructuralPowerContext RootCtx = Session->MakeProcessorContext(EAttachContext::BulkLoad, Root);
			FStructuralPowerTransferGate::RefreshKeyedTransferOnRoot(
				RootCtx,
				Root,
				/*bLocalPromoteOnly=*/false);
		}

		for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : Session->TrackedEndpoints())
		{
			if (Pair.Value.Kind != EStructuralEndpointKind::Switch)
			{
				continue;
			}

			if (AFGBuildableCircuitSwitch* Switch = Pair.Value.GetSwitch())
			{
				FStructuralPowerSwitchProcessor::EnsureListener(BulkCtx, Switch);
			}
		}
	}

	UE_LOG(LogStructuralPower, Log,
		TEXT("[HALSP] Post-load bulk drain complete — %d bridge component root(s)"),
		Roots.Num());

	Session->CrossSite().SeedFeedSignaturesForSites(*Session, Roots);
}
