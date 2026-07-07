// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Connection/FStructuralSwitchConnectionPoint.h"

#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Circuit/FStructuralCircuitPromotionUtil.h"
#include "Components/UFGStructuralPowerConnectionComponent.h"
#include "Config/FStructuralPowerModConfig.h"
#include "Core/EAttachContext.h"
#include "Core/EStructuralPowerRole.h"
#include "Core/EStructuralPowerScope.h"
#include "FGPowerConnectionComponent.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Graph/FStructuralSwitchParentResolver.h"
#include "Core/FStructuralPowerContext.h"
#include "Processors/FStructuralPowerBridgeProcessor.h"
#include "Processors/FStructuralPowerSwitchProcessor.h"
#include "Save/AStructuralPowerGraphSubsystem.h"
#include "StructuralPowerLog.h"

FStructuralSwitchConnectionPoint::FStructuralSwitchConnectionPoint(
	AStructuralPowerGraphSubsystem& InGraph,
	AFGBuildableCircuitSwitch* InSwitch)
	: Graph(InGraph)
	, Switch(InSwitch)
{
}

UFGPowerConnectionComponent* FStructuralSwitchConnectionPoint::GetStructuralConnector()
{
	AFGBuildableCircuitSwitch* SwitchPtr = Switch.Get();
	if (!IsValid(SwitchPtr))
	{
		return nullptr;
	}

	return Graph.GetOrCreateBusConnector(SwitchPtr);
}

FStructuralWallAnchor FStructuralSwitchConnectionPoint::GetStructureAnchor() const
{
	AFGBuildableCircuitSwitch* SwitchPtr = Switch.Get();
	if (!IsValid(SwitchPtr))
	{
		return {};
	}

	const FStructuralOutletParentResolveParams ParentParams = Graph.MakeOutletParentResolveParams();
	const FStructuralSwitchParentResolveResult ParentResolve =
		FStructuralSwitchParentResolver::Resolve(
			SwitchPtr,
			Graph.GetWorld(),
			Graph.StructureGraph,
			Graph.LightweightIndex,
			/*bPreferWirePort=*/false,
			&ParentParams);
	return ParentResolve.Anchor;
}

bool FStructuralSwitchConnectionPoint::ResolveTrackedRoot(
	FStructuralNodeId& OutSwitchId,
	int32& OutRoot)
{
	AFGBuildableCircuitSwitch* SwitchPtr = Switch.Get();
	if (!IsValid(SwitchPtr))
	{
		return false;
	}

	OutSwitchId = Graph.MakeNodeId(SwitchPtr);
	FTrackedEndpoint& Tracked = Graph.TrackedEndpoints.FindOrAdd(OutSwitchId);
	Tracked.Actor = SwitchPtr;
	Tracked.Kind = EStructuralEndpointKind::Switch;
	Graph.RegisterBuildableActor(SwitchPtr);
	FStructuralPowerContext ListenerCtx = Graph.MakeProcessorContext(EAttachContext::WireDelta);
	FStructuralPowerSwitchProcessor::EnsureListener(ListenerCtx, SwitchPtr);

	if (Tracked.ParentId.IsValid())
	{
		OutRoot = Graph.FindRootForTrackedEndpoint(Tracked);
		if (OutRoot != INDEX_NONE)
		{
			return true;
		}
	}

	const FStructuralWallAnchor ParentAnchor = GetStructureAnchor();
	FStructuralNodeId ParentId;
	OutRoot = Graph.ResolveEndpointComponentRoot(SwitchPtr, ParentAnchor, ParentId);
	Tracked.ParentId = ParentId;
	if (OutRoot != INDEX_NONE)
	{
		Graph.MarkBridgeEndpointRootIndexDirty();
	}

	return OutRoot != INDEX_NONE;
}

void FStructuralSwitchConnectionPoint::OnWireOrGateChanged(EAttachContext AttachContext)
{
	AFGBuildableCircuitSwitch* SwitchPtr = Switch.Get();
	if (!IsValid(SwitchPtr) || !FStructuralPowerModConfig::IsGatePowerSwitchesEnabled())
	{
		return;
	}

	if (!SwitchPtr->IsBridgeActive())
	{
		return;
	}

	UFGStructuralPowerConnectionComponent* OutletBus =
		Cast<UFGStructuralPowerConnectionComponent>(GetStructuralConnector());
	if (!OutletBus)
	{
		return;
	}

	FStructuralNodeId SwitchId;
	int32 Root = INDEX_NONE;
	ResolveTrackedRoot(SwitchId, Root);

	FStructuralPowerContext Ctx = Graph.MakeProcessorContext(AttachContext, Root);
	const bool bKeyedSubnet = FStructuralPowerSwitchProcessor::HasAssignedControl(Ctx, SwitchPtr);
	const bool bWiredBridge = FStructuralSwitchParentResolver::IsWiredToStructureSide(SwitchPtr);

	FStructuralPowerBridgeProcessor::ApplyLocalAttachForSwitch(
		Ctx,
		SwitchPtr,
		OutletBus,
		Root,
		SwitchId,
		AttachContext,
		bKeyedSubnet);

	if (AttachContext == EAttachContext::Toggle && bWiredBridge && Root != INDEX_NONE)
	{
		FStructuralPowerBridgeProcessor::PropagateCrossSiteFeedChange(Ctx, SwitchPtr, Root);
	}

	UE_LOG(LogStructuralPower, Log,
		TEXT("[HALSP] switch wire delta %s scope=%s site=%d role=%s attach=%s"
			" root=%d busCircuit=%d powered=%d wired=%d"),
		*SwitchPtr->GetName(),
		StructuralPowerScopeToString(EStructuralPowerScope::Site),
		Root,
		StructuralPowerRoleToString(EStructuralPowerRole::Gateway),
		AttachContextToString(AttachContext),
		Root,
		OutletBus->GetCircuitID(),
		FStructuralCircuitPromotionUtil::ComponentCarriesPower(OutletBus) ? 1 : 0,
		bWiredBridge ? 1 : 0);
}
