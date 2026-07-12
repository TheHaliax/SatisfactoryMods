// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Circuit/FStructuralGraphCircuitOps.h"

#include "Core/FStructuralGraphSession.h"
#include "Save/AStructuralPowerGraphSubsystem.h"

#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableCircuitBridge.h"
#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "Circuit/FStructuralCircuitPromotionScope.h"
#include "Circuit/FStructuralCircuitPromotionUtil.h"
#include "Components/UFGStructuralPowerConnectionComponent.h"
#include "Config/FStructuralPowerModConfig.h"
#include "Diagnostics/FStructuralPowerTrace.h"
#include "Diagnostics/FStructuralPowerTraceScope.h"
#include "Engine/World.h"
#include "FGCircuitConnectionComponent.h"
#include "FGCircuitSubsystem.h"
#include "FGPowerConnectionComponent.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Processors/FStructuralPowerSwitchProcessor.h"
#include "Routing/FStructuralPowerRouter.h"
#include "Buildables/FGBuildablePowerStorage.h"
#include "Rules/FStructuralEligibilityRules.h"
#include "StructuralPowerConstants.h"
#include "StructuralPowerLog.h"

namespace
{
static bool IsBridgeEndpointOutletBus(const UFGStructuralPowerConnectionComponent* Bus)
{
	if (!IsValid(Bus))
	{
		return false;
	}

	const FName BusName = Bus->GetFName();
	if (BusName == StructuralPowerConstants::PanelControlBusConnectorName
		|| BusName == StructuralPowerConstants::SwitchControlBusConnectorName)
	{
		return false;
	}

	const AFGBuildable* Buildable = Cast<AFGBuildable>(Bus->GetOwner());
	if (!IsValid(Buildable))
	{
		return false;
	}

	if (FStructuralEligibilityRules::IsPowerBridgePole(Buildable))
	{
		return true;
	}

	if (FStructuralEligibilityRules::IsPowerBridgeSwitch(Buildable))
	{
		return true;
	}

	return FStructuralEligibilityRules::IsPowerStorage(Buildable);
}
} // namespace

void FStructuralGraphCircuitOps::Bind(FStructuralGraphSession* InSession)
{
	Session = InSession;
}

void FStructuralGraphCircuitOps::BeginCircuitPromotion()
{
	// ConnectComponents fires circuit-changed mid-promotion — defer switch refresh until depth hits 0.
	++Session->CircuitPromotionDepth();
}

void FStructuralGraphCircuitOps::EndCircuitPromotion()
{
	check(Session->CircuitPromotionDepth() > 0);
	--Session->CircuitPromotionDepth();
}

namespace
{
bool LinkHiddenPairImpl(
	FStructuralGraphSession* Session,
	UFGPowerConnectionComponent* A,
	UFGPowerConnectionComponent* B,
	bool bPromoteCircuit,
	ELogVerbosity::Type PromoteVerbosity)
{
	HALSP_TRACE_SCOPE_DETAIL(
		TEXT("mod"),
		TEXT("circuit.LinkHiddenPair"),
		FString::Printf(
			TEXT("promote=%d A=%s B=%s"),
			bPromoteCircuit ? 1 : 0,
			IsValid(A) ? *A->GetName() : TEXT("null"),
			IsValid(B) ? *B->GetName() : TEXT("null")));

	if (!IsValid(A) || !IsValid(B) || A == B)
	{
		return false;
	}

	if (A->HasHiddenConnection(B))
	{
		return false;
	}

	if (!A->IsHidden() && !B->IsHidden())
	{
		return false;
	}

	const int32 CircuitA = A->GetCircuitID();
	const int32 CircuitB = B->GetCircuitID();
	if (CircuitA != INDEX_NONE && CircuitA == CircuitB)
	{
		return false;
	}

	bool bAdded = false;
	const TCHAR* Path = TEXT("?");

	if (A->IsHidden() && B->IsHidden())
	{
		A->AddHiddenConnection(B);
		bAdded = true;
		Path = bPromoteCircuit ? TEXT("mesh_bidir") : TEXT("mesh_bidir_only");
	}
	else if (A->IsHidden())
	{
		A->AddHiddenConnection(B);
		bAdded = true;
		Path = bPromoteCircuit ? TEXT("hidden_A_to_B") : TEXT("hidden_A_to_B_only");
	}
	else if (B->IsHidden())
	{
		B->AddHiddenConnection(A);
		bAdded = true;
		Path = bPromoteCircuit ? TEXT("hidden_B_to_A") : TEXT("hidden_B_to_A_only");
	}
	else
	{
		return false;
	}

	FStructuralPowerTrace::LogLinkOp(TEXT("link"), A, B, bAdded, Path);

	// FG UFGCircuitConnectionComponent::AddHiddenConnection — "Connects both ends … and the
	// circuits." No explicit ConnectComponents here (avoids double merge).
	(void)bPromoteCircuit;
	(void)PromoteVerbosity;
	(void)Session;

	return bAdded;
}
} // namespace

bool FStructuralGraphCircuitOps::LinkHiddenPair(
	UFGPowerConnectionComponent* A,
	UFGPowerConnectionComponent* B,
	bool bPromoteCircuit)
{
	return LinkHiddenPairImpl(
		Session,
		A,
		B,
		bPromoteCircuit,
		ELogVerbosity::Log);
}

bool FStructuralGraphCircuitOps::LinkHiddenPairLocal(
	UFGPowerConnectionComponent* A,
	UFGPowerConnectionComponent* B,
	bool bPromoteCircuit)
{
	return LinkHiddenPairImpl(
		Session,
		A,
		B,
		bPromoteCircuit,
		ELogVerbosity::Verbose);
}

void FStructuralGraphCircuitOps::PromoteStructuralMeshFrom(UFGPowerConnectionComponent* Seed)
{
	// Bulk/runtime: repair split hidden edges only — AddHiddenConnection already merged on add.
	PromoteDirectHiddenLinks(Seed);
}

void FStructuralGraphCircuitOps::PromoteDirectHiddenLinks(UFGPowerConnectionComponent* Seed)
{
	if (!IsValid(Seed))
	{
		return;
	}

	FStructuralCircuitPromotionScope PromotionScope(&Session->Owner());

	TSet<UFGPowerConnectionComponent*> Visited;
	Visited.Add(Seed);
	TArray<UFGPowerConnectionComponent*> Queue;
	Queue.Add(Seed);

	int32 Repaired = 0;
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

			if (FStructuralCircuitPromotionUtil::HiddenLinkNeedsCircuitRepair(Current, Other))
			{
				FStructuralCircuitPromotionUtil::PromoteCircuitLink(
					Current,
					Other,
					ELogVerbosity::Verbose);
				++Repaired;
			}

			if (Other->IsHidden() && !Visited.Contains(Other))
			{
				Visited.Add(Other);
				Queue.Add(Other);
			}
		}
	}

	if (Repaired > 0)
	{
		UE_LOG(LogStructuralPower, Verbose,
			TEXT("[HALSP] hidden mesh repair from %s repaired=%d visited=%d"),
			*Seed->GetName(),
			Repaired,
			Visited.Num());
	}
}

bool FStructuralGraphCircuitOps::IsPanelSupplyLinked(
	UFGPowerConnectionComponent* InputPower,
	UFGPowerConnectionComponent* Feed) const
{
	return IsValid(InputPower) && IsValid(Feed) && InputPower->HasHiddenConnection(Feed);
}

bool FStructuralGraphCircuitOps::IsPanelSupplyLinkedAndLive(
	UFGPowerConnectionComponent* InputPower,
	UFGPowerConnectionComponent* Feed) const
{
	return IsPanelSupplyLinked(InputPower, Feed)
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

	if (InputPower->HasHiddenConnection(Feed))
	{
		FStructuralCircuitPromotionUtil::PromoteCircuitLink(Feed, InputPower);
	}

	UFGPowerConnectionComponent* Seed = FStructuralCircuitPromotionUtil::ConnectorSuppliesPower(InputPower)
		? InputPower
		: (FStructuralCircuitPromotionUtil::ConnectorSuppliesPower(Feed) ? Feed : nullptr);
	if (!Seed)
	{
		if (UFGStructuralPowerConnectionComponent* FeedBus =
				Cast<UFGStructuralPowerConnectionComponent>(Feed))
		{
			Seed = FindPoweredHiddenReachable(FeedBus);
		}
	}

	if (Seed)
	{
		PromoteDirectHiddenLinks(Seed);
	}
	(void)bLocalPromoteOnly;
}

void FStructuralGraphCircuitOps::PromoteOutletBusIfPowered(
	UFGStructuralPowerConnectionComponent* OutletBus,
	bool bLocalPromoteOnly)
{
	if (!IsValid(OutletBus))
	{
		return;
	}

	UFGStructuralPowerConnectionComponent* Seed =
		FStructuralCircuitPromotionUtil::ConnectorSuppliesPower(OutletBus)
		? OutletBus
		: FindPoweredHiddenReachable(OutletBus);
	if (!Seed)
	{
		return;
	}

	PromoteDirectHiddenLinks(Seed);
	(void)bLocalPromoteOnly;
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
	bool bBridgePeersOnly,
	bool bMeshOnlyLinks)
{
	HALSP_TRACE_SCOPE_DETAIL(
		TEXT("mod"),
		TEXT("circuit.TryMeshPeerBus"),
		IsValid(Host) ? Host->GetName() : TEXT("null"));

	if (Root == INDEX_NONE || !IsValid(Host) || !IsValid(OutletBus))
	{
		return false;
	}

	Session->BridgeRootIndex().RefreshBridgeEndpointRootIndex();

	const bool bPromoteCircuit = !bMeshOnlyLinks;
	bool bLinked = false;
	Session->EndpointIndex().ForEachBridgeOnRoot(Root, [&](const FStructuralNodeId& PeerId)
	{
		if (bLinked || PeerId == SelfId)
		{
			return;
		}

		const FTrackedEndpoint* PeerTracked = Session->TrackedEndpoints().Find(PeerId);
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
				&& PeerTracked->Kind != EStructuralEndpointKind::Switch
				&& PeerTracked->Kind != EStructuralEndpointKind::Storage)
			{
				return;
			}
		}
		else if (!Session->Restitch().ShouldMeshEndpoints(Host, SiblingHost, Root))
		{
			return;
		}

		if (!Session->Restitch().ShouldEndpointParticipateInRestitch(SiblingHost, PeerTracked->Kind))
		{
			return;
		}

		if (UFGStructuralPowerConnectionComponent* SiblingBus =
				AStructuralPowerGraphSubsystem::FindBusConnector(SiblingHost))
		{
			// Star-mesh: first eligible peer only. Do not keep scanning when LinkHiddenPair
			// returns false (already linked or same vanilla circuit) — legacy megasites
			// can have thousands of bridge endpoints on one root.
			if (OutletBus->HasHiddenConnection(SiblingBus))
			{
				bLinked = true;
				return;
			}

			const int32 CircuitA = OutletBus->GetCircuitID();
			const int32 CircuitB = SiblingBus->GetCircuitID();
			if (CircuitA != INDEX_NONE && CircuitA == CircuitB)
			{
				bLinked = true;
				return;
			}

			LinkHiddenPairLocal(OutletBus, SiblingBus, bPromoteCircuit);
			bLinked = true;
			return;
		}
	});

	return bLinked;
}

void FStructuralGraphCircuitOps::LinkBusToVisibleConnectionsLocal(
	AFGBuildable* Host,
	UFGStructuralPowerConnectionComponent* Bus,
	bool bMeshOnlyLinks)
{
	HALSP_TRACE_SCOPE_DETAIL(
		TEXT("mod"),
		TEXT("circuit.LinkBusToVisible"),
		IsValid(Host) ? Host->GetName() : TEXT("null"));

	if (!IsValid(Host) || !IsValid(Bus))
	{
		return;
	}

	const bool bPromoteCircuit = !bMeshOnlyLinks;

	auto LinkVisiblePort = [&](UFGPowerConnectionComponent* Visible)
	{
		if (IsValid(Visible) && !Visible->IsHidden())
		{
			LinkHiddenPairLocal(Bus, Visible, bPromoteCircuit);
		}
	};

	if (AFGBuildablePowerPole* Pole = Cast<AFGBuildablePowerPole>(Host))
	{
		for (UFGPowerConnectionComponent* Visible : Pole->GetPowerConnections())
		{
			LinkVisiblePort(Visible);
		}
		return;
	}

	if (AFGBuildableCircuitSwitch* Switch = Cast<AFGBuildableCircuitSwitch>(Host))
	{
		if (UFGPowerConnectionComponent* Conn0 = Cast<UFGPowerConnectionComponent>(Switch->GetConnection0()))
		{
			LinkVisiblePort(Conn0);
		}
		if (UFGPowerConnectionComponent* Conn1 = Cast<UFGPowerConnectionComponent>(Switch->GetConnection1()))
		{
			LinkVisiblePort(Conn1);
		}
		return;
	}

	if (AFGBuildableCircuitBridge* Bridge = Cast<AFGBuildableCircuitBridge>(Host))
	{
		if (UFGCircuitConnectionComponent* Conn0 = Bridge->GetConnection0())
		{
			LinkVisiblePort(Cast<UFGPowerConnectionComponent>(Conn0));
		}
		if (UFGCircuitConnectionComponent* Conn1 = Bridge->GetConnection1())
		{
			LinkVisiblePort(Cast<UFGPowerConnectionComponent>(Conn1));
		}
		return;
	}

	TInlineComponentArray<UFGPowerConnectionComponent*> PowerConns;
	Host->GetComponents(PowerConns);
	for (UFGPowerConnectionComponent* Visible : PowerConns)
	{
		if (!IsValid(Visible) || Visible->IsHidden() || Visible == Bus)
		{
			continue;
		}

		LinkHiddenPairLocal(Bus, Visible, bPromoteCircuit);
	}
}

void FStructuralGraphCircuitOps::LinkBusToVisibleConnections(
	AFGBuildable* Host,
	UFGStructuralPowerConnectionComponent* Bus)
{
	if (Session->BulkLoadDrainActive())
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

		if (Cast<AFGBuildableCircuitSwitch>(Host))
		{
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
			return;
		}

		TInlineComponentArray<UFGPowerConnectionComponent*> PowerConns;
		Host->GetComponents(PowerConns);
		for (UFGPowerConnectionComponent* Visible : PowerConns)
		{
			if (!IsValid(Visible) || Visible->IsHidden() || Visible == Bus)
			{
				continue;
			}

			LinkHiddenPairLocal(Bus, Visible);
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
		UFGStructuralPowerConnectionComponent* Peer =
			Cast<UFGStructuralPowerConnectionComponent>(OtherRaw);
		if (!IsValid(Peer) || Peer == Bus)
		{
			continue;
		}

		const FName PeerName = Peer->GetFName();
		if (PeerName == StructuralPowerConstants::PanelControlBusConnectorName
			|| PeerName == StructuralPowerConstants::SwitchControlBusConnectorName)
		{
			continue;
		}

		if (!IsBridgeEndpointOutletBus(Peer))
		{
			continue;
		}

		return true;
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

	Session->BridgeRootIndex().RefreshBridgeEndpointRootIndex();

	if (const TWeakObjectPtr<UFGCircuitConnectionComponent>* CachedEntry =
			Session->SourceConnectorByRoot().Find(ComponentRoot))
	{
		UFGCircuitConnectionComponent* Cached = CachedEntry->Get();
		const AActor* CachedOwner = IsValid(Cached) ? Cached->GetOwner() : nullptr;
		const bool bCachedIsSwitch =
			IsValid(CachedOwner) && CachedOwner->IsA(AFGBuildableCircuitSwitch::StaticClass());
		if (IsValid(Cached)
			&& !bCachedIsSwitch
			&& CachedOwner != ExcludeHost
			&& FStructuralCircuitPromotionUtil::ComponentCarriesPower(Cast<UFGPowerConnectionComponent>(Cached)))
		{
			return Cached;
		}
		Session->SourceConnectorByRoot().Remove(ComponentRoot);
	}

	UFGPowerConnectionComponent* PoweredVisible = nullptr;
	UFGStructuralPowerConnectionComponent* PoweredBus = nullptr;

	Session->EndpointIndex().ForEachOnRoot(ComponentRoot, [&](const FStructuralNodeId& NodeId)
	{
		const FTrackedEndpoint* Tracked = Session->TrackedEndpoints().Find(NodeId);
		if (!Tracked)
		{
			return;
		}

		AFGBuildable* Host = Tracked->Actor.Get();
		if (!IsValid(Host) || Host == ExcludeHost)
		{
			return;
		}

		if (UFGStructuralPowerConnectionComponent* Bus = AStructuralPowerGraphSubsystem::FindBusConnector(Host))
		{
			if (!PoweredBus && FStructuralCircuitPromotionUtil::ComponentCarriesPower(Bus))
			{
				PoweredBus = Bus;
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
	});

	if (PoweredVisible)
	{
		Session->SourceConnectorByRoot().Add(ComponentRoot, PoweredVisible);
		return PoweredVisible;
	}

	if (PoweredBus)
	{
		Session->SourceConnectorByRoot().Add(ComponentRoot, PoweredBus);
		return PoweredBus;
	}

	Session->EndpointIndex().ForEachOnRoot(ComponentRoot, [&](const FStructuralNodeId& NodeId)
	{
		if (Session->SourceConnectorByRoot().Contains(ComponentRoot))
		{
			return;
		}

		const FTrackedEndpoint* Tracked = Session->TrackedEndpoints().Find(NodeId);
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
				Session->SourceConnectorByRoot().Add(ComponentRoot, Reachable);
			}
		}
	});

	if (const TWeakObjectPtr<UFGCircuitConnectionComponent>* CachedEntry =
			Session->SourceConnectorByRoot().Find(ComponentRoot))
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

	AActor* FeedOwner = SourcePower->GetOwner();
	if (!IsValid(FeedOwner))
	{
		return SourcePower;
	}

	AFGBuildablePowerPole* Pole = Cast<AFGBuildablePowerPole>(FeedOwner);
	if (!IsValid(Pole))
	{
		return SourcePower;
	}

	if (UFGStructuralPowerConnectionComponent* Bus =
			AStructuralPowerGraphSubsystem::FindBusConnector(Pole))
	{
		return Bus;
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

	const FStructuralComponentKey CompKey = Session->Ids().MakeComponentKeyForRoot(ComponentRoot);
	if (!CompKey.IsValid())
	{
		return nullptr;
	}

	const FName DefaultId = Session->Ids().GetOrCreateComponentDefaultId(CompKey);

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

	for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : Session->TrackedEndpoints())
	{
		if (Pair.Value.Kind != EStructuralEndpointKind::Switch)
		{
			continue;
		}

		AFGBuildableCircuitSwitch* Switch =
			Cast<AFGBuildableCircuitSwitch>(Pair.Value.Actor.Get());
		if (!IsValid(Switch)
			|| Session->StructureGraph().FindRoot(Pair.Value.ParentId) != ComponentRoot)
		{
			continue;
		}

		const FName SwitchControl = Session->Ids().ResolveControl(Switch, EStructuralChannel::Switch);
		if (!FStructuralPowerRouter::ShouldRouteSwitchGate(
				SwitchControl,
				DeviceKey.Source,
				CompKey,
				CompKey))
		{
			continue;
		}

		if (UFGStructuralPowerConnectionComponent* ControlBus =
				AStructuralPowerGraphSubsystem::FindSwitchControlBus(Switch))
		{
			if (FStructuralPowerSwitchProcessor::ShouldInjectStructuralPath(Switch))
			{
				return ControlBus;
			}

			return nullptr;
		}

		return nullptr;
	}

	if (DeviceKey.Source != DefaultId)
	{
		return nullptr;
	}

	return ResolveDefaultFeed();
}
