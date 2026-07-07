// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Connection/FStructuralPanelConnectionPoint.h"

#include "Buildables/FGBuildableLightsControlPanel.h"
#include "Circuit/FStructuralCircuitPromotionUtil.h"
#include "Config/FStructuralPowerModConfig.h"
#include "Core/EAttachContext.h"
#include "Core/EStructuralPowerRole.h"
#include "Core/EStructuralPowerScope.h"
#include "FGPowerConnectionComponent.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Panel/FStructuralPanelPortResolver.h"
#include "Core/FStructuralPowerContext.h"
#include "Processors/FStructuralPowerBridgeProcessor.h"
#include "Save/AStructuralPowerGraphSubsystem.h"
#include "StructuralPowerLog.h"

FStructuralPanelConnectionPoint::FStructuralPanelConnectionPoint(
	AStructuralPowerGraphSubsystem& InGraph,
	AFGBuildableLightsControlPanel* InPanel)
	: Graph(InGraph)
	, Panel(InPanel)
{
}

UFGPowerConnectionComponent* FStructuralPanelConnectionPoint::GetStructuralConnector()
{
	AFGBuildableLightsControlPanel* PanelPtr = Panel.Get();
	if (!IsValid(PanelPtr))
	{
		return nullptr;
	}

	FStructuralPanelPorts Ports;
	if (!FStructuralPanelPortResolver::Resolve(PanelPtr, Ports))
	{
		return nullptr;
	}

	return FStructuralPanelPortResolver::AsPowerConnection(Ports.Input);
}

FStructuralWallAnchor FStructuralPanelConnectionPoint::GetStructureAnchor() const
{
	AFGBuildableLightsControlPanel* PanelPtr = Panel.Get();
	if (!IsValid(PanelPtr))
	{
		return {};
	}

	return Graph.ResolveOutletAnchor(PanelPtr);
}

void FStructuralPanelConnectionPoint::OnWireOrGateChanged(EAttachContext AttachContext)
{
	AFGBuildableLightsControlPanel* PanelPtr = Panel.Get();
	if (!IsValid(PanelPtr) || !FStructuralPowerModConfig::IsGroupLightingEnabled())
	{
		return;
	}

	FStructuralPanelPorts Ports;
	if (!FStructuralPanelPortResolver::Resolve(PanelPtr, Ports))
	{
		return;
	}

	const FStructuralWallAnchor ParentAnchor = GetStructureAnchor();
	FStructuralNodeId ParentId;
	const int32 Root = Graph.ResolveEndpointComponentRoot(PanelPtr, ParentAnchor, ParentId);

	const FStructuralNodeId PanelId = Graph.MakeNodeId(PanelPtr);
	FTrackedEndpoint& Tracked = Graph.TrackedEndpoints.FindOrAdd(PanelId);
	Tracked.Actor = PanelPtr;
	Tracked.ParentId = ParentId;
	Tracked.Kind = EStructuralEndpointKind::Panel;
	Graph.RegisterBuildableActor(PanelPtr);
	Graph.EnsurePanelListener(PanelPtr);
	if (Root != INDEX_NONE)
	{
		Graph.MarkBridgeEndpointRootIndexDirty();
	}

	if (AttachContext == EAttachContext::Toggle)
	{
		Tracked.bPanelLinksReady = false;
		Tracked.bDownstreamLinksReady = false;
	}

	const bool bLocalPromoteOnly = AttachContext != EAttachContext::Toggle;
	FStructuralPowerContext Ctx = Graph.MakeProcessorContext(AttachContext, Root);
	FStructuralPowerBridgeProcessor::ApplyLocalAttachForPanel(
		Ctx,
		PanelPtr,
		bLocalPromoteOnly);

	const UFGPowerConnectionComponent* InputPower =
		FStructuralPanelPortResolver::AsPowerConnection(Ports.Input);
	UE_LOG(LogStructuralPower, Log,
		TEXT("[HALSP] panel wire delta %s scope=%s site=%d role=%s attach=%s"
			" root=%d inputPowered=%d"),
		*PanelPtr->GetName(),
		StructuralPowerScopeToString(EStructuralPowerScope::Site),
		Root,
		StructuralPowerRoleToString(EStructuralPowerRole::Router),
		AttachContextToString(AttachContext),
		Root,
		InputPower && FStructuralCircuitPromotionUtil::ComponentCarriesPower(InputPower) ? 1 : 0);
}
