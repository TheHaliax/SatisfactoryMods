// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Circuit/FStructuralGraphCircuitOps.h"

#include "Save/AStructuralPowerGraphSubsystem.h"

#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableCircuitBridge.h"
#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "Circuit/FStructuralCircuitPromotionScope.h"
#include "Circuit/FStructuralCircuitPromotionUtil.h"
#include "Circuit/FStructuralHiddenConnectionUtil.h"
#include "Components/UFGStructuralPowerConnectionComponent.h"
#include "Config/FStructuralPowerModConfig.h"
#include "Diagnostics/FStructuralPowerTrace.h"
#include "Engine/World.h"
#include "FGCircuitConnectionComponent.h"
#include "FGCircuitSubsystem.h"
#include "FGPowerConnectionComponent.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Processors/FStructuralPowerSwitchProcessor.h"
#include "Routing/FStructuralPowerRouter.h"
#include "StructuralPowerLog.h"

void FStructuralGraphCircuitOps::Bind(AStructuralPowerGraphSubsystem* InSubsystem)
{
	Subsystem = InSubsystem;
}

void FStructuralGraphCircuitOps::BeginCircuitPromotion()
{
	// ConnectComponents fires circuit-changed mid-promotion — defer switch refresh until depth hits 0.
	++Subsystem->CircuitPromotionDepth;
}

void FStructuralGraphCircuitOps::EndCircuitPromotion()
{
	check(Subsystem->CircuitPromotionDepth > 0);
	--Subsystem->CircuitPromotionDepth;
}

bool FStructuralGraphCircuitOps::LinkHiddenPair(
	UFGPowerConnectionComponent* A,
	UFGPowerConnectionComponent* B)
{
	if (!IsValid(A) || !IsValid(B) || A == B)
	{
		return false;
	}

	if (A->HasHiddenConnection(B))
	{
		FStructuralCircuitPromotionScope PromotionScope(Subsystem);
		FStructuralCircuitPromotionUtil::PromoteCircuitLink(A, B, ELogVerbosity::Verbose);
		return false;
	}

	bool bAdded = false;
	const TCHAR* Path = TEXT("?");

	if (A->IsHidden() && B->IsHidden())
	{
		const int32 CircuitA = A->GetCircuitID();
		const int32 CircuitB = B->GetCircuitID();
		if (CircuitA != INDEX_NONE && CircuitA == CircuitB)
		{
			return false;
		}

		bAdded = FStructuralHiddenConnectionUtil::AddMeshOnlyHiddenLink(A, B);
		Path = TEXT("mesh_bidir");
	}
	else if (A->IsHidden())
	{
		A->AddHiddenConnection(B);
		bAdded = true;
		Path = TEXT("hidden_A_to_B");
	}
	else if (B->IsHidden())
	{
		B->AddHiddenConnection(A);
		bAdded = true;
		Path = TEXT("hidden_B_to_A");
	}
	else
	{
		return false;
	}

	FStructuralPowerTrace::LogLinkOp(TEXT("link"), A, B, bAdded, Path);

	if (bAdded)
	{
		FStructuralCircuitPromotionScope PromotionScope(Subsystem);
		FStructuralCircuitPromotionUtil::PromoteCircuitLink(A, B);
	}

	return bAdded;
}

bool FStructuralGraphCircuitOps::LinkHiddenPairLocal(
	UFGPowerConnectionComponent* A,
	UFGPowerConnectionComponent* B)
{
	if (!IsValid(A) || !IsValid(B) || A == B)
	{
		return false;
	}

	if (A->HasHiddenConnection(B))
	{
		return false;
	}

	bool bAdded = false;
	const TCHAR* Path = TEXT("?");

	if (A->IsHidden() && B->IsHidden())
	{
		const int32 CircuitA = A->GetCircuitID();
		const int32 CircuitB = B->GetCircuitID();
		if (CircuitA != INDEX_NONE && CircuitA == CircuitB)
		{
			return false;
		}

		bAdded = FStructuralHiddenConnectionUtil::AddMeshOnlyHiddenLink(A, B);
		Path = TEXT("mesh_bidir");
	}
	else if (A->IsHidden())
	{
		A->AddHiddenConnection(B);
		bAdded = true;
		Path = TEXT("hidden_A_to_B");
	}
	else if (B->IsHidden())
	{
		B->AddHiddenConnection(A);
		bAdded = true;
		Path = TEXT("hidden_B_to_A");
	}
	else
	{
		return false;
	}

	FStructuralPowerTrace::LogLinkOp(TEXT("link"), A, B, bAdded, Path);

	if (bAdded)
	{
		FStructuralCircuitPromotionScope PromotionScope(Subsystem);
		FStructuralCircuitPromotionUtil::PromoteCircuitLink(A, B, ELogVerbosity::Verbose);
	}

	return bAdded;
}

void FStructuralGraphCircuitOps::PromoteStructuralMeshFrom(UFGPowerConnectionComponent* Seed)
{
	if (!IsValid(Seed) || !FStructuralCircuitPromotionUtil::ComponentCarriesPower(Seed))
	{
		return;
	}

	if (Subsystem->bBulkLoadDrainActive)
	{
		PromoteDirectHiddenLinks(Seed);
		return;
	}

	UWorld* World = Subsystem->GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	AFGCircuitSubsystem* CircuitSubsystem = AFGCircuitSubsystem::Get(World);
	if (!IsValid(CircuitSubsystem))
	{
		return;
	}

	FStructuralCircuitPromotionScope PromotionScope(Subsystem);

	TSet<UFGPowerConnectionComponent*> Visited;
	Visited.Add(Seed);
	TArray<UFGPowerConnectionComponent*> Queue;
	Queue.Add(Seed);

	int32 EdgesPromoted = 0;
	int32 EdgesSkipped = 0;
	int32 QueueHead = 0;

	while (QueueHead < Queue.Num())
	{
		UFGPowerConnectionComponent* Current = Queue[QueueHead++];

		TArray<UFGCircuitConnectionComponent*> HiddenLinks;
		Current->GetHiddenConnections(HiddenLinks);

		for (UFGCircuitConnectionComponent* OtherRaw : HiddenLinks)
		{
			UFGPowerConnectionComponent* Other = Cast<UFGPowerConnectionComponent>(OtherRaw);
			if (!IsValid(Other))
			{
				continue;
			}

			const int32 CircuitCurrent = Current->GetCircuitID();
			const int32 CircuitOther = Other->GetCircuitID();
			if (CircuitCurrent != INDEX_NONE && CircuitCurrent == CircuitOther)
			{
				++EdgesSkipped;
			}
			else
			{
				CircuitSubsystem->ConnectComponents(Current, Other);
				++EdgesPromoted;
			}

			if (Other->IsHidden() && !Visited.Contains(Other))
			{
				Visited.Add(Other);
				Queue.Add(Other);
			}
		}
	}

	if (EdgesPromoted > 0 || EdgesSkipped > 0)
	{
		UE_LOG(LogStructuralPower, Verbose,
			TEXT("[HALSP] mesh flood from %s promoted=%d skipped=%d visited=%d"),
			*Seed->GetName(),
			EdgesPromoted,
			EdgesSkipped,
			Visited.Num());
	}
}

void FStructuralGraphCircuitOps::PromoteDirectHiddenLinks(UFGPowerConnectionComponent* Seed)
{
	if (!IsValid(Seed) || !FStructuralCircuitPromotionUtil::ComponentCarriesPower(Seed))
	{
		return;
	}

	FStructuralCircuitPromotionScope PromotionScope(Subsystem);

	TArray<UFGCircuitConnectionComponent*> HiddenLinks;
	Seed->GetHiddenConnections(HiddenLinks);

	for (UFGCircuitConnectionComponent* OtherRaw : HiddenLinks)
	{
		UFGPowerConnectionComponent* Other = Cast<UFGPowerConnectionComponent>(OtherRaw);
		if (!IsValid(Other))
		{
			continue;
		}

		FStructuralCircuitPromotionUtil::PromoteCircuitLink(Seed, Other, ELogVerbosity::Verbose);
	}
}

bool FStructuralGraphCircuitOps::IsPanelSupplyLinkedAndLive(
	UFGPowerConnectionComponent* InputPower,
	UFGPowerConnectionComponent* Feed) const
{
	return IsValid(InputPower)
		&& IsValid(Feed)
		&& InputPower->HasHiddenConnection(Feed)
		&& FStructuralCircuitPromotionUtil::ConnectorSuppliesPower(InputPower);
}

void FStructuralGraphCircuitOps::PromotePanelSupplyConnection(
	UFGPowerConnectionComponent* InputPower,
	UFGPowerConnectionComponent* Feed,
	bool bLocalPromoteOnly)
{
	if (!IsValid(InputPower) || !IsValid(Feed))
	{
		return;
	}

	if (!FStructuralCircuitPromotionUtil::ConnectorSuppliesPower(InputPower))
	{
		FStructuralCircuitPromotionUtil::PromoteCircuitLink(Feed, InputPower);
	}

	UFGPowerConnectionComponent* Seed = FStructuralCircuitPromotionUtil::ConnectorSuppliesPower(InputPower)
		? InputPower
		: (FStructuralCircuitPromotionUtil::ComponentCarriesPower(Feed) ? Feed : nullptr);
	if (!Seed)
	{
		if (UFGStructuralPowerConnectionComponent* FeedBus =
				Cast<UFGStructuralPowerConnectionComponent>(Feed))
		{
			Seed = FindPoweredHiddenReachable(FeedBus);
		}
	}

	if (!Seed)
	{
		return;
	}

	if (bLocalPromoteOnly || Subsystem->bBulkLoadDrainActive)
	{
		PromoteDirectHiddenLinks(Seed);
	}
	else
	{
		PromoteStructuralMeshFrom(Seed);
	}
}

void FStructuralGraphCircuitOps::PromoteOutletBusIfPowered(
	UFGStructuralPowerConnectionComponent* OutletBus,
	bool bLocalPromoteOnly)
{
	if (!IsValid(OutletBus))
	{
		return;
	}

	UFGStructuralPowerConnectionComponent* Seed = FStructuralCircuitPromotionUtil::ComponentCarriesPower(OutletBus)
		? OutletBus
		: FindPoweredHiddenReachable(OutletBus);
	if (!Seed)
	{
		return;
	}

	if (Subsystem->bBulkLoadDrainActive || bLocalPromoteOnly)
	{
		PromoteDirectHiddenLinks(Seed);
	}
	else
	{
		PromoteStructuralMeshFrom(Seed);
	}
}

void FStructuralGraphCircuitOps::ApplyLocalBridgeBusAttach(
	AFGBuildable* Host,
	UFGStructuralPowerConnectionComponent* OutletBus,
	int32 Root,
	const FStructuralNodeId& SelfId,
	const AFGBuildable* FeedExcludeHost)
{
	if (Root == INDEX_NONE)
	{
		PromoteOutletBusIfPowered(OutletBus, /*bLocalPromoteOnly=*/true);
		return;
	}

	if (FStructuralPowerModConfig::IsPowerSwitchManualGroupsEnabled()
		&& Cast<AFGBuildableCircuitSwitch>(Host))
	{
		UFGPowerConnectionComponent* FeedPower = nullptr;
		if (AFGBuildableCircuitSwitch* Switch = Cast<AFGBuildableCircuitSwitch>(Host))
		{
			if (UFGCircuitConnectionComponent* Conn0 = Switch->GetConnection0())
			{
				if (UFGPowerConnectionComponent* Visible =
						Cast<UFGPowerConnectionComponent>(Conn0))
				{
					if (IsValid(Visible) && FStructuralCircuitPromotionUtil::ComponentCarriesPower(Visible))
					{
						FeedPower = Visible;
					}
				}
			}
			if (!FeedPower)
			{
				if (UFGCircuitConnectionComponent* Conn1 = Switch->GetConnection1())
				{
					if (UFGPowerConnectionComponent* Visible =
							Cast<UFGPowerConnectionComponent>(Conn1))
					{
						if (IsValid(Visible) && FStructuralCircuitPromotionUtil::ComponentCarriesPower(Visible))
						{
							FeedPower = Visible;
						}
					}
				}
			}
		}

		if (!FeedPower)
		{
			if (UFGCircuitConnectionComponent* Feed =
					GetComponentSourceConnector(Root, FeedExcludeHost))
			{
				FeedPower = Cast<UFGPowerConnectionComponent>(Feed);
			}
		}

		if (FeedPower)
		{
			LinkHiddenPairLocal(OutletBus, FeedPower);
		}
	}

	if (!HasBridgeBusPeerMesh(OutletBus))
	{
		TryMeshPeerBusOnComponent(
			Host,
			OutletBus,
			Root,
			SelfId,
			/*bBridgePeersOnly=*/true);
	}
}

bool FStructuralGraphCircuitOps::TryMeshPeerBusOnComponent(
	AFGBuildable* Host,
	UFGStructuralPowerConnectionComponent* OutletBus,
	int32 Root,
	const FStructuralNodeId& SelfId,
	bool bBridgePeersOnly)
{
	if (Root == INDEX_NONE || !IsValid(Host) || !IsValid(OutletBus))
	{
		return false;
	}

	Subsystem->BridgeRootIndex.RefreshBridgeEndpointRootIndex();

	bool bLinked = false;
	Subsystem->EndpointIndex.ForEachBridgeOnRoot(Root, [&](const FStructuralNodeId& PeerId)
	{
		if (bLinked || PeerId == SelfId)
		{
			return;
		}

		const FTrackedEndpoint* PeerTracked = Subsystem->TrackedEndpoints.Find(PeerId);
		if (!PeerTracked)
		{
			return;
		}

		AFGBuildable* SiblingHost = PeerTracked->Actor.Get();
		if (!IsValid(SiblingHost))
		{
			return;
		}

		if (bBridgePeersOnly)
		{
			if (PeerTracked->Kind != EStructuralEndpointKind::Pole
				&& PeerTracked->Kind != EStructuralEndpointKind::Switch)
			{
				return;
			}
		}
		else if (!Subsystem->RestitchOps.ShouldMeshEndpoints(Host, SiblingHost, Root))
		{
			return;
		}

		if (!Subsystem->RestitchOps.ShouldEndpointParticipateInRestitch(SiblingHost, PeerTracked->Kind))
		{
			return;
		}

		if (UFGStructuralPowerConnectionComponent* SiblingBus = AStructuralPowerGraphSubsystem::FindBusConnector(SiblingHost))
		{
			bLinked = LinkHiddenPairLocal(OutletBus, SiblingBus);
		}
	});

	return bLinked;
}

void FStructuralGraphCircuitOps::LinkBusToVisibleConnectionsLocal(
	AFGBuildable* Host,
	UFGStructuralPowerConnectionComponent* Bus)
{
	if (!IsValid(Host) || !IsValid(Bus))
	{
		return;
	}

	if (AFGBuildablePowerPole* Pole = Cast<AFGBuildablePowerPole>(Host))
	{
		for (UFGPowerConnectionComponent* Visible : Pole->GetPowerConnections())
		{
			if (!IsValid(Visible) || Visible->IsHidden())
			{
				continue;
			}

			LinkHiddenPairLocal(Bus, Visible);
		}
		return;
	}

	if (AFGBuildableCircuitBridge* Bridge = Cast<AFGBuildableCircuitBridge>(Host))
	{
		if (UFGCircuitConnectionComponent* Conn0 = Bridge->GetConnection0())
		{
			if (UFGPowerConnectionComponent* Visible = Cast<UFGPowerConnectionComponent>(Conn0))
			{
				LinkHiddenPairLocal(Bus, Visible);
			}
		}
		if (UFGCircuitConnectionComponent* Conn1 = Bridge->GetConnection1())
		{
			if (UFGPowerConnectionComponent* Visible = Cast<UFGPowerConnectionComponent>(Conn1))
			{
				LinkHiddenPairLocal(Bus, Visible);
			}
		}
	}
}

void FStructuralGraphCircuitOps::LinkBusToVisibleConnections(
	AFGBuildable* Host,
	UFGStructuralPowerConnectionComponent* Bus)
{
	if (Subsystem->bBulkLoadDrainActive)
	{
		if (!IsValid(Host) || !IsValid(Bus))
		{
			return;
		}

		if (AFGBuildablePowerPole* Pole = Cast<AFGBuildablePowerPole>(Host))
		{
			for (UFGPowerConnectionComponent* Visible : Pole->GetPowerConnections())
			{
				if (!IsValid(Visible) || Visible->IsHidden())
				{
					continue;
				}

				LinkHiddenPairLocal(Bus, Visible);
			}
			return;
		}

		if (AFGBuildableCircuitBridge* Bridge = Cast<AFGBuildableCircuitBridge>(Host))
		{
			if (UFGCircuitConnectionComponent* Conn0 = Bridge->GetConnection0())
			{
				if (UFGPowerConnectionComponent* Visible = Cast<UFGPowerConnectionComponent>(Conn0))
				{
					LinkHiddenPairLocal(Bus, Visible);
				}
			}
			if (UFGCircuitConnectionComponent* Conn1 = Bridge->GetConnection1())
			{
				if (UFGPowerConnectionComponent* Visible = Cast<UFGPowerConnectionComponent>(Conn1))
				{
					LinkHiddenPairLocal(Bus, Visible);
				}
			}
		}

		return;
	}

	LinkBusToVisibleConnectionsLocal(Host, Bus);
}

bool FStructuralGraphCircuitOps::HasBridgeBusPeerMesh(
	UFGStructuralPowerConnectionComponent* Bus) const
{
	if (!IsValid(Bus))
	{
		return false;
	}

	TArray<UFGCircuitConnectionComponent*> HiddenLinks;
	Bus->GetHiddenConnections(HiddenLinks);
	for (UFGCircuitConnectionComponent* OtherRaw : HiddenLinks)
	{
		if (Cast<UFGStructuralPowerConnectionComponent>(OtherRaw))
		{
			return true;
		}
	}

	return false;
}

UFGStructuralPowerConnectionComponent* FStructuralGraphCircuitOps::FindPoweredHiddenReachable(
	UFGStructuralPowerConnectionComponent* StartHidden,
	int32 MaxHiddenHops) const
{
	if (!IsValid(StartHidden))
	{
		return nullptr;
	}

	if (FStructuralCircuitPromotionUtil::ComponentCarriesPower(StartHidden))
	{
		return StartHidden;
	}

	TSet<UFGPowerConnectionComponent*> Visited;
	TArray<UFGPowerConnectionComponent*> Queue;
	Queue.Add(StartHidden);
	Visited.Add(StartHidden);

	int32 QueueHead = 0;
	while (QueueHead < Queue.Num() && Visited.Num() <= MaxHiddenHops)
	{
		UFGPowerConnectionComponent* Current = Queue[QueueHead++];
		if (FStructuralCircuitPromotionUtil::ComponentCarriesPower(Current))
		{
			return Cast<UFGStructuralPowerConnectionComponent>(Current);
		}

		TArray<UFGCircuitConnectionComponent*> HiddenLinks;
		Current->GetHiddenConnections(HiddenLinks);
		for (UFGCircuitConnectionComponent* OtherRaw : HiddenLinks)
		{
			UFGPowerConnectionComponent* Other = Cast<UFGPowerConnectionComponent>(OtherRaw);
			if (!IsValid(Other) || !Other->IsHidden() || Visited.Contains(Other))
			{
				continue;
			}

			Visited.Add(Other);
			Queue.Add(Other);
		}
	}

	return nullptr;
}

UFGCircuitConnectionComponent* FStructuralGraphCircuitOps::GetComponentSourceConnector(
	int32 ComponentRoot,
	const AFGBuildable* ExcludeHost)
{
	if (ComponentRoot == INDEX_NONE)
	{
		return nullptr;
	}

	Subsystem->BridgeRootIndex.RefreshBridgeEndpointRootIndex();

	if (const TWeakObjectPtr<UFGCircuitConnectionComponent>* CachedEntry =
			Subsystem->SourceConnectorByRoot.Find(ComponentRoot))
	{
		UFGCircuitConnectionComponent* Cached = CachedEntry->Get();
		if (IsValid(Cached)
			&& Cached->GetOwner() != ExcludeHost
			&& FStructuralCircuitPromotionUtil::ComponentCarriesPower(Cast<UFGPowerConnectionComponent>(Cached)))
		{
			return Cached;
		}
		Subsystem->SourceConnectorByRoot.Remove(ComponentRoot);
	}

	UFGPowerConnectionComponent* PoweredVisible = nullptr;
	UFGStructuralPowerConnectionComponent* PoweredBus = nullptr;

	Subsystem->EndpointIndex.ForEachOnRoot(ComponentRoot, [&](const FStructuralNodeId& NodeId)
	{
		const FTrackedEndpoint* Tracked = Subsystem->TrackedEndpoints.Find(NodeId);
		if (!Tracked)
		{
			return;
		}

		AFGBuildable* Host = Tracked->Actor.Get();
		if (!IsValid(Host) || Host == ExcludeHost)
		{
			return;
		}

		// Switch outlet buses are torn down on toggle — never use them as feed donors.
		if (Tracked->Kind != EStructuralEndpointKind::Switch)
		{
			if (UFGStructuralPowerConnectionComponent* Bus = AStructuralPowerGraphSubsystem::FindBusConnector(Host))
			{
				if (!PoweredBus && FStructuralCircuitPromotionUtil::ComponentCarriesPower(Bus))
				{
					PoweredBus = Bus;
				}
			}
		}

		if (AFGBuildablePowerPole* Pole = Cast<AFGBuildablePowerPole>(Host))
		{
			for (UFGPowerConnectionComponent* Visible : Pole->GetPowerConnections())
			{
				if (IsValid(Visible) && !Visible->IsHidden() && FStructuralCircuitPromotionUtil::ComponentCarriesPower(Visible))
				{
					PoweredVisible = Visible;
					return;
				}
			}
		}
		else if (AFGBuildableCircuitSwitch* Switch = Cast<AFGBuildableCircuitSwitch>(Host))
		{
			if (FStructuralPowerModConfig::IsGatePowerSwitchesEnabled() && Switch->IsBridgeActive())
			{
				if (UFGCircuitConnectionComponent* Conn0 = Switch->GetConnection0())
				{
					if (UFGPowerConnectionComponent* Visible = Cast<UFGPowerConnectionComponent>(Conn0))
					{
						if (IsValid(Visible) && FStructuralCircuitPromotionUtil::ComponentCarriesPower(Visible))
						{
							PoweredVisible = Visible;
							return;
						}
					}
				}
				if (!PoweredVisible)
				{
					if (UFGCircuitConnectionComponent* Conn1 = Switch->GetConnection1())
					{
						if (UFGPowerConnectionComponent* Visible = Cast<UFGPowerConnectionComponent>(Conn1))
						{
							if (IsValid(Visible) && FStructuralCircuitPromotionUtil::ComponentCarriesPower(Visible))
							{
								PoweredVisible = Visible;
							}
						}
					}
				}
			}
		}
	});

	if (PoweredVisible)
	{
		Subsystem->SourceConnectorByRoot.Add(ComponentRoot, PoweredVisible);
		return PoweredVisible;
	}

	if (PoweredBus)
	{
		Subsystem->SourceConnectorByRoot.Add(ComponentRoot, PoweredBus);
		return PoweredBus;
	}

	Subsystem->EndpointIndex.ForEachOnRoot(ComponentRoot, [&](const FStructuralNodeId& NodeId)
	{
		if (Subsystem->SourceConnectorByRoot.Contains(ComponentRoot))
		{
			return;
		}

		const FTrackedEndpoint* Tracked = Subsystem->TrackedEndpoints.Find(NodeId);
		if (!Tracked)
		{
			return;
		}

		AFGBuildable* Host = Tracked->Actor.Get();
		if (!IsValid(Host) || Host == ExcludeHost
			|| Tracked->Kind == EStructuralEndpointKind::Switch)
		{
			return;
		}

		if (UFGStructuralPowerConnectionComponent* Bus = AStructuralPowerGraphSubsystem::FindBusConnector(Host))
		{
			if (UFGStructuralPowerConnectionComponent* Reachable = FindPoweredHiddenReachable(Bus))
			{
				Subsystem->SourceConnectorByRoot.Add(ComponentRoot, Reachable);
			}
		}
	});

	if (const TWeakObjectPtr<UFGCircuitConnectionComponent>* CachedEntry =
			Subsystem->SourceConnectorByRoot.Find(ComponentRoot))
	{
		return CachedEntry->Get();
	}

	return nullptr;
}

static UFGPowerConnectionComponent* RedirectFeedToHiddenBus(UFGPowerConnectionComponent* SourcePower)
{
	if (!IsValid(SourcePower) || SourcePower->IsHidden())
	{
		return SourcePower;
	}

	if (AActor* FeedOwner = SourcePower->GetOwner())
	{
		if (UFGStructuralPowerConnectionComponent* Bus =
				AStructuralPowerGraphSubsystem::FindBusConnector(Cast<AFGBuildable>(FeedOwner)))
		{
			return Bus;
		}
	}

	return SourcePower;
}

UFGPowerConnectionComponent* FStructuralGraphCircuitOps::ResolveSubnetFeedConnector(
	int32 ComponentRoot,
	const FStructuralChannelKey& DeviceKey)
{
	if (ComponentRoot == INDEX_NONE || DeviceKey.Source.IsNone())
	{
		return nullptr;
	}

	const FStructuralComponentKey CompKey = Subsystem->IdOps.MakeComponentKeyForRoot(ComponentRoot);
	if (!CompKey.IsValid())
	{
		return nullptr;
	}

	const FName DefaultId = Subsystem->IdOps.GetOrCreateComponentDefaultId(CompKey);

	auto ResolveDefaultFeed = [&]() -> UFGPowerConnectionComponent*
	{
		UFGCircuitConnectionComponent* Source =
			GetComponentSourceConnector(ComponentRoot, nullptr);
		return RedirectFeedToHiddenBus(Cast<UFGPowerConnectionComponent>(Source));
	};

	if (DeviceKey.Source == DefaultId)
	{
		return ResolveDefaultFeed();
	}

	if (FStructuralPowerModConfig::IsPowerSwitchManualGroupsEnabled())
	{
		for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : Subsystem->TrackedEndpoints)
		{
			if (Pair.Value.Kind != EStructuralEndpointKind::Switch)
			{
				continue;
			}

			AFGBuildableCircuitSwitch* Switch =
				Cast<AFGBuildableCircuitSwitch>(Pair.Value.Actor.Get());
			if (!IsValid(Switch)
				|| Subsystem->StructureGraph.FindRoot(Pair.Value.ParentId) != ComponentRoot)
			{
				continue;
			}

			const FName SwitchControl = Subsystem->IdOps.ResolveControl(Switch, EStructuralChannel::Switch);
			if (!FStructuralPowerRouter::ShouldRouteSwitchGate(
					SwitchControl,
					DeviceKey.Source,
					CompKey,
					CompKey))
			{
				continue;
			}

			if (!FStructuralPowerSwitchProcessor::ShouldInjectStructuralPath(Switch))
			{
				continue;
			}

			if (UFGStructuralPowerConnectionComponent* Bus = AStructuralPowerGraphSubsystem::FindBusConnector(Switch))
			{
				return Bus;
			}
		}

		return nullptr;
	}

	FStructuralChannelKey FeedKey = DeviceKey;
	if (FStructuralPowerRouter::ShouldRouteChannelLink(DeviceKey, FeedKey, CompKey, CompKey))
	{
		return ResolveDefaultFeed();
	}

	return nullptr;
}
