// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Graph/FStructuralSwitchParentResolver.h"

#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Buildables/FGBuildableFactory.h"
#include "Buildables/FGBuildableWire.h"
#include "FGCircuitConnectionComponent.h"
#include "Graph/FStructuralAttachmentResolver.h"
#include "Rules/FStructuralEligibilityRules.h"
#include "StructuralPowerLog.h"

namespace
{
static AFGBuildable* GetVisibleNeighborBuildable(UFGCircuitConnectionComponent* Port)
{
	if (!IsValid(Port))
	{
		return nullptr;
	}

	TArray<AFGBuildableWire*> Wires;
	Port->GetWires(Wires);
	for (AFGBuildableWire* Wire : Wires)
	{
		if (!IsValid(Wire))
		{
			continue;
		}

		UFGCircuitConnectionComponent* End0 = Wire->GetConnection(0);
		UFGCircuitConnectionComponent* End1 = Wire->GetConnection(1);
		UFGCircuitConnectionComponent* Other = (End0 == Port) ? End1 : End0;
		if (!IsValid(Other) || Other == Port)
		{
			continue;
		}

		if (AFGBuildable* Neighbor = Cast<AFGBuildable>(Other->GetOwner()))
		{
			return Neighbor;
		}
	}

	return nullptr;
}

static bool IsGridSideNeighbor(const AFGBuildable* Neighbor)
{
	return IsValid(Neighbor) && Neighbor->IsA<AFGBuildableFactory>();
}

static FStructuralWallAnchor AnchorFromStructureNeighbor(
	AFGBuildable* Neighbor,
	UWorld* World,
	const FStructuralLightweightIndex& LightweightIndex)
{
	if (!IsValid(Neighbor))
	{
		return {};
	}

	if (FStructuralEligibilityRules::IsBusMember(Neighbor))
	{
		FStructuralWallAnchor Anchor;
		Anchor.Actor = Neighbor;
		Anchor.WorldLocation = Neighbor->GetActorLocation();
		return Anchor;
	}

	if (FStructuralEligibilityRules::IsPowerBridgePole(Neighbor)
		|| FStructuralEligibilityRules::IsPowerBridgeSwitch(Neighbor))
	{
		return FStructuralAttachmentResolver::ResolveStructuralParent(
			Neighbor,
			World,
			LightweightIndex);
	}

	return {};
}

static FStructuralSwitchParentResolveResult TryResolveFromWiredPorts(
	AFGBuildableCircuitSwitch* Switch,
	UWorld* World,
	const FStructuralLightweightIndex& LightweightIndex)
{
	FStructuralSwitchParentResolveResult Result;

	for (int32 PortIndex = 0; PortIndex < 2; ++PortIndex)
	{
		UFGCircuitConnectionComponent* Port = PortIndex == 0
			? Switch->GetConnection0()
			: Switch->GetConnection1();
		if (!IsValid(Port) || Port->GetNumConnections() <= 0)
		{
			continue;
		}

		AFGBuildable* Neighbor = GetVisibleNeighborBuildable(Port);
		if (!IsValid(Neighbor) || IsGridSideNeighbor(Neighbor))
		{
			continue;
		}

		const FStructuralWallAnchor Anchor = AnchorFromStructureNeighbor(
			Neighbor,
			World,
			LightweightIndex);
		if (!Anchor.IsValid())
		{
			continue;
		}

		Result.Anchor = Anchor;
		Result.WirePortIndex = PortIndex;
		return Result;
	}

	return Result;
}
}

FStructuralSwitchParentResolveResult FStructuralSwitchParentResolver::Resolve(
	AFGBuildableCircuitSwitch* Switch,
	UWorld* World,
	const FStructuralConnectivityGraph& Graph,
	const FStructuralLightweightIndex& LightweightIndex)
{
	FStructuralSwitchParentResolveResult Result;
	if (!IsValid(Switch) || !IsValid(World))
	{
		return Result;
	}

	(void)Graph;

	Result.Anchor = FStructuralAttachmentResolver::ResolveStructuralParent(
		Switch,
		World,
		LightweightIndex);
	if (Result.IsValid())
	{
		UE_LOG(LogStructuralPower, Log,
			TEXT("[PWR] switch %s parent resolved via mount"),
			*Switch->GetName());
		return Result;
	}

	Result = TryResolveFromWiredPorts(Switch, World, LightweightIndex);
	if (Result.IsValid())
	{
		UE_LOG(LogStructuralPower, Log,
			TEXT("[PWR] switch %s parent resolved via wire_port_%c neighbor=%s"),
			*Switch->GetName(),
			Result.WirePortIndex == 0 ? TEXT('A') : TEXT('B'),
			IsValid(Result.Anchor.Actor)
				? *Result.Anchor.Actor->GetName()
				: (Result.Anchor.Lightweight.IsValid()
					? *Result.Anchor.Lightweight.BuildableClass->GetName()
					: TEXT("?")));
	}

	return Result;
}
