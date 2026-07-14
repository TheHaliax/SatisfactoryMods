// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Routing/EStructuralChannel.h"

class AFGBuildable;
class AFGBuildableLightsControlPanel;
class AFGBuildablePowerPole;
class FStructuralGraphSession;
class UFGCircuitConnectionComponent;
class UFGPowerConnectionComponent;
class UFGStructuralPowerConnectionComponent;
struct FStructuralChannelKey;
struct FStructuralNodeId;

class STRUCTURALPOWER_API FStructuralGraphCircuitOps
{
public:
	FStructuralGraphCircuitOps() = default;

	void Bind(FStructuralGraphSession* InSession);

	void BeginCircuitPromotion();
	void EndCircuitPromotion();

	bool LinkHiddenPair(
		UFGPowerConnectionComponent* A,
		UFGPowerConnectionComponent* B,
		bool bPromoteCircuit = true);
	bool LinkHiddenPairLocal(
		UFGPowerConnectionComponent* A,
		UFGPowerConnectionComponent* B,
		bool bPromoteCircuit = true);

	void PromoteStructuralMeshFrom(UFGPowerConnectionComponent* Seed);
	void PromoteDirectHiddenLinks(UFGPowerConnectionComponent* Seed);

	bool IsPanelSupplyLinked(
		UFGPowerConnectionComponent* InputPower,
		UFGPowerConnectionComponent* Feed) const;
	bool IsPanelSupplyLinkedAndLive(
		UFGPowerConnectionComponent* InputPower,
		UFGPowerConnectionComponent* Feed) const;

	void PromotePanelSupplyConnection(
		UFGPowerConnectionComponent* InputPower,
		UFGPowerConnectionComponent* Feed,
		bool bLocalPromoteOnly = false);

	void PromoteOutletBusIfPowered(
		UFGStructuralPowerConnectionComponent* OutletBus,
		bool bLocalPromoteOnly = false);

	void ApplyLocalBridgeBusAttach(
		AFGBuildable* Host,
		UFGStructuralPowerConnectionComponent* OutletBus,
		int32 Root,
		const FStructuralNodeId& SelfId,
		const AFGBuildable* FeedExcludeHost = nullptr);

	bool TryMeshPeerBusOnComponent(
		AFGBuildable* Host,
		UFGStructuralPowerConnectionComponent* OutletBus,
		int32 Root,
		const FStructuralNodeId& SelfId,
		bool bBridgePeersOnly,
		bool bMeshOnlyLinks = false);

	void LinkBusToVisibleConnectionsLocal(
		AFGBuildable* Host,
		UFGStructuralPowerConnectionComponent* Bus,
		bool bMeshOnlyLinks = false);

	void LinkBusToVisibleConnections(
		AFGBuildable* Host,
		UFGStructuralPowerConnectionComponent* Bus);

	bool HasBridgeBusPeerMesh(UFGStructuralPowerConnectionComponent* Bus) const;

	UFGCircuitConnectionComponent* GetComponentSourceConnector(
		int32 ComponentRoot,
		const AFGBuildable* ExcludeHost = nullptr);

	UFGPowerConnectionComponent* ResolveSubnetFeedConnector(
		int32 ComponentRoot,
		const FStructuralChannelKey& DeviceKey);

	UFGStructuralPowerConnectionComponent* FindPoweredHiddenReachable(
		UFGStructuralPowerConnectionComponent* StartHidden,
		int32 MaxHiddenHops = 512) const;

	bool DoesComponentRootCarryPower(int32 ComponentRoot) const;
	bool DoesSiteStructuralBusCarryPower(int32 ComponentRoot) const;

private:
	FStructuralGraphSession* Session = nullptr;
};
