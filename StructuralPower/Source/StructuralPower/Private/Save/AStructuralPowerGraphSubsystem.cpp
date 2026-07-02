// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Save/AStructuralPowerGraphSubsystem.h"

#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "Circuit/FStructuralHiddenConnectionUtil.h"
#include "Components/UFGStructuralPowerConnectionComponent.h"
#include "Diagnostics/FStructuralPowerDiagnostics.h"
#include "Diagnostics/FStructuralPowerTrace.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FGBuildableSubsystem.h"
#include "FGCircuitSubsystem.h"
#include "FGLightweightBuildableSubsystem.h"
#include "FGPowerConnectionComponent.h"
#include "Graph/FOverlapBoxAdjacencyStrategy.h"
#include "Lightweight/FStructuralLightweightIndex.h"
#include "Rules/FStructuralEligibilityRules.h"
#include "Session/FStructuralPowerSessionSettings.h"
#include "StructuralPowerConstants.h"
#include "StructuralPowerLog.h"

namespace
{
static bool ComponentCarriesPower(const UFGPowerConnectionComponent* Component)
{
	if (!IsValid(Component))
	{
		return false;
	}

	if (Component->GetCircuitID() != INDEX_NONE || Component->IsConnected())
	{
		return true;
	}

	return !Component->IsHidden() && Component->GetNumConnections() > 0;
}

static void PromoteCircuitLink(UFGPowerConnectionComponent* A, UFGPowerConnectionComponent* B)
{
	if (!IsValid(A) || !IsValid(B))
	{
		return;
	}

	if (!ComponentCarriesPower(A) && !ComponentCarriesPower(B))
	{
		return;
	}

	if (UWorld* World = A->GetWorld())
	{
		if (AFGCircuitSubsystem* CircuitSubsystem = AFGCircuitSubsystem::Get(World))
		{
			CircuitSubsystem->ConnectComponents(A, B);
			FStructuralPowerTrace::LogLinkOp(TEXT("promote"), A, B, true, TEXT("ConnectComponents"));
		}
	}
}
}

AStructuralPowerGraphSubsystem::AStructuralPowerGraphSubsystem()
{
	bReplicates = false;
}

AStructuralPowerGraphSubsystem* AStructuralPowerGraphSubsystem::GetOrCreate(UWorld* World)
{
	if (!IsValid(World))
	{
		return nullptr;
	}

	for (TActorIterator<AStructuralPowerGraphSubsystem> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			return *It;
		}
	}

	if (World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = TEXT("StructuralPowerGraphSubsystem");
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	return World->SpawnActor<AStructuralPowerGraphSubsystem>(
		AStructuralPowerGraphSubsystem::StaticClass(),
		FTransform::Identity,
		SpawnParams);
}

FStructuralNodeId AStructuralPowerGraphSubsystem::MakeNodeId(const AFGBuildable* Buildable)
{
	FStructuralNodeId Id;
	if (!IsValid(Buildable))
	{
		return Id;
	}

	Id.BuildableClass = FSoftClassPath(Buildable->GetClass());
	Id.ActorName = Buildable->GetFName();
	return Id;
}

bool AStructuralPowerGraphSubsystem::HasStructuralConnector(const AFGBuildable* Buildable)
{
	return FindStructuralConnector(Buildable) != nullptr
		|| (Cast<AFGBuildablePowerPole>(Buildable) && FindOutletBusConnector(Cast<AFGBuildablePowerPole>(Buildable)) != nullptr);
}

UFGStructuralPowerConnectionComponent* AStructuralPowerGraphSubsystem::FindStructuralConnector(const AFGBuildable* Buildable)
{
	if (!IsValid(Buildable))
	{
		return nullptr;
	}

	TInlineComponentArray<UFGStructuralPowerConnectionComponent*> Connectors;
	const_cast<AFGBuildable*>(Buildable)->GetComponents(Connectors);
	for (UFGStructuralPowerConnectionComponent* Connector : Connectors)
	{
		if (IsValid(Connector) && Connector->GetFName() == StructuralPowerConstants::StructuralConnectorName)
		{
			return Connector;
		}
	}

	return nullptr;
}

UFGStructuralPowerConnectionComponent* AStructuralPowerGraphSubsystem::FindOutletBusConnector(const AFGBuildablePowerPole* Outlet)
{
	if (!IsValid(Outlet))
	{
		return nullptr;
	}

	TInlineComponentArray<UFGStructuralPowerConnectionComponent*> Connectors;
	const_cast<AFGBuildablePowerPole*>(Outlet)->GetComponents(Connectors);
	for (UFGStructuralPowerConnectionComponent* Connector : Connectors)
	{
		if (IsValid(Connector) && Connector->GetFName() == StructuralPowerConstants::OutletBusConnectorName)
		{
			return Connector;
		}
	}

	return nullptr;
}

void AStructuralPowerGraphSubsystem::OnWorldReady(UWorld* World)
{
	if (!IsValid(World) || World->GetNetMode() == NM_Client)
	{
		return;
	}

	if (!bPostLoadRebuilt)
	{
		RebuildRuntimeFromSave();
		bPostLoadRebuilt = true;
	}

	UE_LOG(LogStructuralPower, Log,
		TEXT("Graph subsystem ready — %d saved nodes, %d tracked LW, %d pending jobs"),
		RuntimeGraph.Num(),
		LightweightIndex.GetTrackedCount(),
		GetPendingJobCount());
}

void AStructuralPowerGraphSubsystem::EnqueueLightweightPlacement(
	const FStructuralLightweightKey& Key,
	bool bDefer)
{
	if (!Key.IsValid() || !FStructuralPowerSessionSettings::IsPropagationEnabled())
	{
		return;
	}

	if (bDefer)
	{
		PendingLightweightJobs.Add(Key);
		UE_LOG(LogStructuralPower, Log,
			TEXT("[PWR] LW queued %s[%d]"),
			*Key.BuildableClass->GetName(),
			Key.Index);
		return;
	}

	ProcessLightweightStructure(Key);
}

void AStructuralPowerGraphSubsystem::EnqueuePlacement(
	AFGBuildable* Buildable,
	EStructuralPlacementJobType JobType,
	bool bDefer)
{
	if (!IsValid(Buildable))
	{
		return;
	}

	if (!Buildable->HasAuthority())
	{
		FStructuralPowerTrace::LogPlacementSkip(Buildable, TEXT("no_authority"));
		return;
	}

	if (!FStructuralPowerSessionSettings::IsPropagationEnabled())
	{
		FStructuralPowerTrace::LogPlacementSkip(Buildable, TEXT("propagation_disabled"));
		return;
	}

	if (bDefer)
	{
		FDeferredPlacementJob Job;
		Job.Buildable = Buildable;
		Job.JobType = JobType;
		PendingJobs.Add(Job);
		FStructuralPowerTrace::LogHook(
			Buildable,
			TEXT("EnqueuePlacement"),
			JobType == EStructuralPlacementJobType::Outlet ? TEXT("queued_outlet") : TEXT("queued_structure"),
			TEXT("defer"));
		return;
	}

	if (JobType == EStructuralPlacementJobType::Outlet)
	{
		ProcessOutlet(Buildable);
	}
	else
	{
		ProcessStructure(Buildable);
	}
}

void AStructuralPowerGraphSubsystem::TickDeferredPlacements(int32 MaxJobs)
{
	if (!FStructuralPowerSessionSettings::IsPropagationEnabled() || MaxJobs <= 0)
	{
		return;
	}

	int32 Processed = 0;
	while (Processed < MaxJobs && PendingLightweightJobs.Num() > 0)
	{
		const FStructuralLightweightKey Key = PendingLightweightJobs[0];
		PendingLightweightJobs.RemoveAt(0);
		ProcessLightweightStructure(Key);
		++Processed;
	}

	while (Processed < MaxJobs && PendingJobs.Num() > 0)
	{
		const FDeferredPlacementJob Job = PendingJobs[0];
		PendingJobs.RemoveAt(0);

		if (AFGBuildable* Buildable = Job.Buildable.Get())
		{
			if (Job.JobType == EStructuralPlacementJobType::Outlet)
			{
				ProcessOutlet(Buildable);
			}
			else
			{
				ProcessStructure(Buildable);
			}
		}

		++Processed;
	}
}

UFGStructuralPowerConnectionComponent* AStructuralPowerGraphSubsystem::GetOrCreateStructuralConnector(AFGBuildable* Buildable)
{
	if (UFGStructuralPowerConnectionComponent* Existing = FindStructuralConnector(Buildable))
	{
		return Existing;
	}

	UFGStructuralPowerConnectionComponent* Connector = NewObject<UFGStructuralPowerConnectionComponent>(
		Buildable,
		StructuralPowerConstants::StructuralConnectorName);
	if (!Connector)
	{
		return nullptr;
	}

	Connector->SetMobility(EComponentMobility::Static);
	Buildable->AddInstanceComponent(Connector);
	Connector->RegisterComponent();
	return Connector;
}

UFGStructuralPowerConnectionComponent* AStructuralPowerGraphSubsystem::GetOrCreateOutletBusConnector(AFGBuildablePowerPole* Outlet)
{
	if (UFGStructuralPowerConnectionComponent* Existing = FindOutletBusConnector(Outlet))
	{
		return Existing;
	}

	UFGStructuralPowerConnectionComponent* Connector = NewObject<UFGStructuralPowerConnectionComponent>(
		Outlet,
		StructuralPowerConstants::OutletBusConnectorName);
	if (!Connector)
	{
		return nullptr;
	}

	Connector->SetMobility(EComponentMobility::Static);
	Outlet->AddInstanceComponent(Connector);
	Connector->RegisterComponent();
	return Connector;
}

FStructuralWallAnchor AStructuralPowerGraphSubsystem::ResolveOutletAnchor(AFGBuildable* Outlet) const
{
	if (!IsValid(Outlet))
	{
		return {};
	}

	if (AFGBuildable* ParentActor = Cast<AFGBuildable>(Outlet->GetParentBuildableActor()))
	{
		if (FStructuralEligibilityRules::IsValidOutletParent(ParentActor))
		{
			return FStructuralWallAnchor{ParentActor, {}, ParentActor->GetActorLocation()};
		}
	}

	if (FStructuralWallAnchor LightweightAnchor = LightweightIndex.FindParentWallForOutlet(Outlet);
		LightweightAnchor.IsValid())
	{
		return LightweightAnchor;
	}

	return {};
}

UFGStructuralPowerConnectionComponent* AStructuralPowerGraphSubsystem::FindStructureHidden(
	const FStructuralWallAnchor& Anchor) const
{
	if (IsValid(Anchor.Actor))
	{
		return FindStructuralConnector(Anchor.Actor);
	}

	if (Anchor.Lightweight.IsValid())
	{
		return LightweightIndex.FindHiddenConnector(Anchor.Lightweight);
	}

	return nullptr;
}

UFGStructuralPowerConnectionComponent* AStructuralPowerGraphSubsystem::GetOrCreateStructureHidden(
	const FStructuralWallAnchor& Anchor)
{
	if (IsValid(Anchor.Actor))
	{
		return GetOrCreateStructuralConnector(Anchor.Actor);
	}

	if (Anchor.Lightweight.IsValid())
	{
		return LightweightIndex.GetOrCreateHiddenConnector(GetWorld(), Anchor.Lightweight, Anchor.WorldLocation);
	}

	return nullptr;
}

UFGStructuralPowerConnectionComponent* AStructuralPowerGraphSubsystem::ResolveNodeConnector(
	const FStructuralNodeId& NodeId,
	bool bIsOutlet)
{
	if (AFGBuildable* Buildable = ResolveBuildable(NodeId))
	{
		if (bIsOutlet)
		{
			return GetOrCreateOutletBusConnector(Cast<AFGBuildablePowerPole>(Buildable));
		}
		return GetOrCreateStructuralConnector(Buildable);
	}

	if (NodeId.IsLightweight())
	{
		const FStructuralLightweightKey Key = FStructuralLightweightIndex::KeyFromNodeId(NodeId);
		if (UFGStructuralPowerConnectionComponent* Existing = LightweightIndex.FindHiddenConnector(Key))
		{
			return Existing;
		}
		return LightweightIndex.GetOrCreateHiddenConnector(GetWorld(), Key, FVector::ZeroVector);
	}

	return nullptr;
}

bool AStructuralPowerGraphSubsystem::LinkHiddenPair(
	UFGPowerConnectionComponent* A,
	UFGPowerConnectionComponent* B)
{
	if (!IsValid(A) || !IsValid(B) || A == B)
	{
		FStructuralPowerTrace::LogLinkOp(TEXT("link"), A, B, false, TEXT("invalid"));
		return false;
	}

	if (A->HasHiddenConnection(B))
	{
		PromoteCircuitLink(A, B);
		FStructuralPowerTrace::LogLinkOp(TEXT("link"), A, B, false, TEXT("already_linked"));
		return false;
	}

	bool bAdded = false;
	const TCHAR* Path = TEXT("?");

	if (A->IsHidden() && B->IsHidden())
	{
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
		FStructuralPowerTrace::LogLinkOp(TEXT("link"), A, B, false, TEXT("neither_hidden"));
		return false;
	}

	FStructuralPowerTrace::LogLinkOp(TEXT("link"), A, B, bAdded, Path);

	if (bAdded)
	{
		PromoteCircuitLink(A, B);
	}

	return bAdded;
}

void AStructuralPowerGraphSubsystem::RegisterGraphEdge(
	const FStructuralNodeId& A,
	const FStructuralNodeId& B,
	bool bAIsOutlet,
	bool bBIsOutlet)
{
	if (!A.IsValid() || !B.IsValid())
	{
		return;
	}

	FStructuralNodeRecord& RecordA = RuntimeGraph.FindOrAdd(A);
	RecordA.NodeId = A;
	RecordA.bIsOutlet = bAIsOutlet;
	RecordA.NeighborIds.AddUnique(B);

	FStructuralNodeRecord& RecordB = RuntimeGraph.FindOrAdd(B);
	RecordB.NodeId = B;
	RecordB.bIsOutlet = bBIsOutlet;
	RecordB.NeighborIds.AddUnique(A);
}

void AStructuralPowerGraphSubsystem::RemoveGraphNode(const FStructuralNodeId& NodeId)
{
	if (!NodeId.IsValid())
	{
		return;
	}

	if (FStructuralNodeRecord* Record = RuntimeGraph.Find(NodeId))
	{
		for (const FStructuralNodeId& NeighborId : Record->NeighborIds)
		{
			if (FStructuralNodeRecord* Neighbor = RuntimeGraph.Find(NeighborId))
			{
				Neighbor->NeighborIds.Remove(NodeId);
			}
		}
	}

	RuntimeGraph.Remove(NodeId);
}

void AStructuralPowerGraphSubsystem::ProcessStructure(AFGBuildable* Buildable)
{
	if (!FStructuralEligibilityRules::IsBusMember(Buildable))
	{
		FStructuralPowerTrace::LogPlacementSkip(Buildable, TEXT("not_bus_member"));
		return;
	}

	UFGStructuralPowerConnectionComponent* SelfHidden = GetOrCreateStructuralConnector(Buildable);
	if (!SelfHidden)
	{
		FStructuralPowerTrace::LogPlacementSkip(Buildable, TEXT("connector_create_failed"));
		return;
	}

	FStructuralPowerTrace::LogConnector(TEXT("structure_hidden"), SelfHidden);

	const FStructuralNodeId SelfId = MakeNodeId(Buildable);
	FStructuralNodeRecord& SelfRecord = RuntimeGraph.FindOrAdd(SelfId);
	SelfRecord.NodeId = SelfId;
	SelfRecord.WorldLocation = Buildable->GetActorLocation();
	SelfRecord.bIsOutlet = false;

	TArray<AFGBuildable*> Neighbors;
	FOverlapBoxAdjacencyStrategy::FindModManagedNeighbors(Buildable, Neighbors);

	int32 LinksCreated = 0;
	for (AFGBuildable* Neighbor : Neighbors)
	{
		UFGStructuralPowerConnectionComponent* NeighborHidden = FindStructuralConnector(Neighbor);
		if (!NeighborHidden)
		{
			continue;
		}

		if (LinkHiddenPair(SelfHidden, NeighborHidden))
		{
			RegisterGraphEdge(SelfId, MakeNodeId(Neighbor), false, false);
			++LinksCreated;
		}
	}

	FVector Origin;
	FVector Extent;
	Buildable->GetActorBounds(false, Origin, Extent);
	const FBox ActorBounds(Origin - Extent, Origin + Extent);

	TArray<FStructuralLightweightKey> LightweightNeighbors;
	LightweightIndex.FindModManagedNeighborsInBounds(ActorBounds, LightweightNeighbors);
	for (const FStructuralLightweightKey& NeighborKey : LightweightNeighbors)
	{
		UFGStructuralPowerConnectionComponent* NeighborHidden = LightweightIndex.FindHiddenConnector(NeighborKey);
		if (!NeighborHidden)
		{
			NeighborHidden = LightweightIndex.GetOrCreateHiddenConnector(
				GetWorld(),
				NeighborKey,
				FVector::ZeroVector);
		}

		if (LinkHiddenPair(SelfHidden, NeighborHidden))
		{
			RegisterGraphEdge(SelfId, FStructuralLightweightIndex::MakeNodeId(NeighborKey), false, false);
			++LinksCreated;
		}
	}

	UE_LOG(LogStructuralPower, Log,
		TEXT("[PWR] structure %s linked %d mod neighbor(s) circuitId=%d"),
		*Buildable->GetName(),
		LinksCreated,
		SelfHidden->GetCircuitID());
}

void AStructuralPowerGraphSubsystem::ProcessLightweightStructure(const FStructuralLightweightKey& Key)
{
	if (!Key.IsValid())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	if (!LightweightIndex.RegisterTrackedMember(World, Key))
	{
		UE_LOG(LogStructuralPower, Warning,
			TEXT("[PWR] LW register failed %s[%d]"),
			*Key.BuildableClass->GetName(),
			Key.Index);
		return;
	}

	FVector ConnectorLocation = FVector::ZeroVector;
	if (AFGLightweightBuildableSubsystem* LwSubsystem = AFGLightweightBuildableSubsystem::Get(World))
	{
		if (const TArray<FRuntimeBuildableInstanceData>* Instances =
			LwSubsystem->GetAllLightweightBuildableInstances().Find(Key.BuildableClass))
		{
			if (Instances->IsValidIndex(Key.Index))
			{
				const FRuntimeBuildableInstanceData& Data = (*Instances)[Key.Index];
				const FBox Bounds = Data.BoundingBox.IsValid
					? Data.BoundingBox.TransformBy(Data.Transform)
					: FBox(Data.Transform.GetLocation(), Data.Transform.GetLocation());
				ConnectorLocation = Bounds.GetCenter();
			}
		}
	}

	UFGStructuralPowerConnectionComponent* SelfHidden = LightweightIndex.GetOrCreateHiddenConnector(
		World,
		Key,
		ConnectorLocation);
	if (!SelfHidden)
	{
		return;
	}

	FStructuralPowerTrace::LogConnector(TEXT("lw_structure_hidden"), SelfHidden);

	const FStructuralNodeId SelfId = FStructuralLightweightIndex::MakeNodeId(Key);
	FStructuralNodeRecord& SelfRecord = RuntimeGraph.FindOrAdd(SelfId);
	SelfRecord.NodeId = SelfId;
	SelfRecord.WorldLocation = ConnectorLocation;
	SelfRecord.bIsOutlet = false;

	const int32 LinksCreated = LightweightIndex.StitchPlacementNeighbors(
		World,
		Key,
		[this](
			UFGStructuralPowerConnectionComponent* A,
			UFGStructuralPowerConnectionComponent* B,
			const FStructuralLightweightKey& KeyA,
			const FStructuralLightweightKey& KeyB)
		{
			if (!LinkHiddenPair(A, B))
			{
				return false;
			}

			RegisterGraphEdge(
				FStructuralLightweightIndex::MakeNodeId(KeyA),
				FStructuralLightweightIndex::MakeNodeId(KeyB),
				false,
				false);
			return true;
		});

	TArray<UFGCircuitConnectionComponent*> HiddenLinks;
	SelfHidden->GetHiddenConnections(HiddenLinks);
	for (UFGCircuitConnectionComponent* OtherRaw : HiddenLinks)
	{
		UFGPowerConnectionComponent* NeighborHidden = Cast<UFGPowerConnectionComponent>(OtherRaw);
		if (!ComponentCarriesPower(NeighborHidden))
		{
			continue;
		}

		PromoteCircuitLink(SelfHidden, NeighborHidden);
		TSet<UFGPowerConnectionComponent*> PoweredNodes;
		PromoteStructuralMeshFrom(NeighborHidden, PoweredNodes);
		break;
	}

	UE_LOG(LogStructuralPower, Log,
		TEXT("[PWR] LW %s[%d] linked %d mod neighbor(s) circuitId=%d tracked=%d"),
		*Key.BuildableClass->GetName(),
		Key.Index,
		LinksCreated,
		SelfHidden->GetCircuitID(),
		LightweightIndex.GetTrackedCount());
}

void AStructuralPowerGraphSubsystem::ProcessOutlet(AFGBuildable* Buildable)
{
	if (!FStructuralEligibilityRules::IsPowerBridgePole(Buildable))
	{
		FStructuralPowerTrace::LogPlacementSkip(Buildable, TEXT("not_bridge_pole"));
		return;
	}

	AFGBuildablePowerPole* Pole = Cast<AFGBuildablePowerPole>(Buildable);
	UFGStructuralPowerConnectionComponent* OutletBus = GetOrCreateOutletBusConnector(Pole);
	if (!OutletBus)
	{
		FStructuralPowerTrace::LogPlacementSkip(Buildable, TEXT("outlet_bus_create_failed"));
		return;
	}

	AActor* ParentActor = Buildable->GetParentBuildableActor();
	AFGBuildable* ParentBuildable = Cast<AFGBuildable>(ParentActor);
	const FStructuralWallAnchor ParentAnchor = ResolveOutletAnchor(Buildable);
	if (!ParentAnchor.IsValid())
	{
		const FString ParentName = IsValid(ParentActor) ? ParentActor->GetName() : TEXT("null");
		UE_LOG(LogStructuralPower, Warning,
			TEXT("[PWR] outlet %s NO_PARENT parent=%s trackedLW=%d"),
			*Buildable->GetName(),
			*ParentName,
			LightweightIndex.GetTrackedCount());
		FStructuralPowerTrace::LogOutletBridge(Buildable, false, INDEX_NONE, OutletBus->GetCircuitID(), 0, TEXT("no_mod_parent"));
		return;
	}

	UFGStructuralPowerConnectionComponent* ParentHidden = GetOrCreateStructureHidden(ParentAnchor);
	if (!ParentHidden)
	{
		FStructuralPowerTrace::LogOutletBridge(Buildable, false, INDEX_NONE, OutletBus->GetCircuitID(), 0, TEXT("parent_hidden_failed"));
		return;
	}

	const FStructuralNodeId OutletId = MakeNodeId(Buildable);
	const FStructuralNodeId ParentId = IsValid(ParentAnchor.Actor)
		? MakeNodeId(ParentAnchor.Actor)
		: FStructuralLightweightIndex::MakeNodeId(ParentAnchor.Lightweight);

	FStructuralNodeRecord& OutletRecord = RuntimeGraph.FindOrAdd(OutletId);
	OutletRecord.NodeId = OutletId;
	OutletRecord.WorldLocation = Buildable->GetActorLocation();
	OutletRecord.bIsOutlet = true;

	const bool bStructLinked = LinkHiddenPair(OutletBus, ParentHidden);
	if (bStructLinked)
	{
		RegisterGraphEdge(OutletId, ParentId, true, false);
	}

	if (ComponentCarriesPower(ParentHidden))
	{
		SyncBridgePoleToPoweredParent(Pole, OutletBus, ParentHidden);
	}

	int32 VisibleLinks = 0;
	int32 WiredVisible = 0;
	for (UFGPowerConnectionComponent* Visible : Pole->GetPowerConnections())
	{
		if (!IsValid(Visible) || Visible->IsHidden())
		{
			continue;
		}

		if (Visible->GetNumConnections() == 0)
		{
			continue;
		}

		++WiredVisible;

		if (LinkHiddenPair(OutletBus, Visible))
		{
			++VisibleLinks;
		}
	}

	if (WiredVisible > 0)
	{
		TSet<UFGPowerConnectionComponent*> PoweredNodes;
		PromoteCircuitLink(OutletBus, ParentHidden);
		PromoteStructuralMeshFrom(ParentHidden, PoweredNodes);
	}

	FStructuralPowerTrace::LogOutletBridge(
		Buildable,
		bStructLinked || VisibleLinks > 0,
		ParentHidden->GetCircuitID(),
		OutletBus->GetCircuitID(),
		WiredVisible,
		bStructLinked ? TEXT("ok") : TEXT("struct_link_failed"));

	UE_LOG(LogStructuralPower, Log,
		TEXT("[PWR] outlet %s parent=%s structLink=%d visibleLinks=%d busCircuit=%d parentCircuit=%d"),
		*Buildable->GetName(),
		IsValid(ParentAnchor.Actor) ? *ParentAnchor.Actor->GetName()
			: *FString::Printf(TEXT("LW:%s[%d]"), *ParentAnchor.Lightweight.BuildableClass->GetName(), ParentAnchor.Lightweight.Index),
		bStructLinked ? 1 : 0,
		VisibleLinks,
		OutletBus->GetCircuitID(),
		ParentHidden->GetCircuitID());
}

void AStructuralPowerGraphSubsystem::ProcessWallOutletAfterWire(AFGBuildablePowerPole* Pole)
{
	if (!IsValid(Pole) || !Pole->HasAuthority())
	{
		return;
	}

	if (!FStructuralPowerSessionSettings::IsPropagationEnabled())
	{
		return;
	}

	FStructuralPowerTrace::LogHook(Pole, TEXT("OnPowerConnectionChanged"), TEXT("wire_refresh"), TEXT("defer"));
	EnqueuePlacement(Pole, EStructuralPlacementJobType::Outlet, /*bDefer=*/true);
}

void AStructuralPowerGraphSubsystem::SyncBridgePoleToPoweredParent(
	AFGBuildablePowerPole* Pole,
	UFGStructuralPowerConnectionComponent* OutletBus,
	UFGStructuralPowerConnectionComponent* ParentHidden)
{
	if (!IsValid(Pole) || !IsValid(OutletBus) || !IsValid(ParentHidden))
	{
		return;
	}

	if (!ComponentCarriesPower(ParentHidden) && !ComponentCarriesPower(OutletBus))
	{
		return;
	}

	PromoteCircuitLink(OutletBus, ParentHidden);

	int32 VisibleSynced = 0;
	for (UFGPowerConnectionComponent* Visible : Pole->GetPowerConnections())
	{
		if (!IsValid(Visible) || Visible->IsHidden())
		{
			continue;
		}

		if (LinkHiddenPair(OutletBus, Visible))
		{
			++VisibleSynced;
		}
		else if (OutletBus->HasHiddenConnection(Visible))
		{
			PromoteCircuitLink(OutletBus, Visible);
			++VisibleSynced;
		}
	}

	UE_LOG(LogStructuralPower, Log,
		TEXT("[PWR] bridge pole %s passive sync parentCircuit=%d busCircuit=%d visibleSynced=%d"),
		*Pole->GetName(),
		ParentHidden->GetCircuitID(),
		OutletBus->GetCircuitID(),
		VisibleSynced);
}

void AStructuralPowerGraphSubsystem::RefreshBridgePolesOnMesh(
	const TSet<UFGPowerConnectionComponent*>& PoweredHiddenNodes)
{
	if (PoweredHiddenNodes.Num() == 0)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(World);
	if (!IsValid(BuildableSubsystem))
	{
		return;
	}

	int32 Refreshed = 0;
	for (AFGBuildable* Buildable : BuildableSubsystem->GetAllBuildablesRef())
	{
		if (!FStructuralEligibilityRules::IsPowerBridgePole(Buildable))
		{
			continue;
		}

		AFGBuildablePowerPole* Pole = Cast<AFGBuildablePowerPole>(Buildable);
		UFGStructuralPowerConnectionComponent* OutletBus = FindOutletBusConnector(Pole);
		if (!OutletBus)
		{
			continue;
		}

		const FStructuralWallAnchor ParentAnchor = ResolveOutletAnchor(Buildable);
		if (!ParentAnchor.IsValid())
		{
			continue;
		}

		UFGStructuralPowerConnectionComponent* ParentHidden = FindStructureHidden(ParentAnchor);
		if (!IsValid(ParentHidden) || !PoweredHiddenNodes.Contains(ParentHidden))
		{
			continue;
		}

		SyncBridgePoleToPoweredParent(Pole, OutletBus, ParentHidden);
		++Refreshed;
	}

	if (Refreshed > 0)
	{
		UE_LOG(LogStructuralPower, Log,
			TEXT("[PWR] refreshed %d bridge pole(s) on powered mesh"),
			Refreshed);
	}
}

void AStructuralPowerGraphSubsystem::PromoteStructuralMeshFrom(UFGPowerConnectionComponent* Seed)
{
	TSet<UFGPowerConnectionComponent*> PoweredNodes;
	PromoteStructuralMeshFrom(Seed, PoweredNodes);
}

void AStructuralPowerGraphSubsystem::PromoteStructuralMeshFrom(
	UFGPowerConnectionComponent* Seed,
	TSet<UFGPowerConnectionComponent*>& OutVisited)
{
	if (!IsValid(Seed))
	{
		return;
	}

	if (!ComponentCarriesPower(Seed))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	AFGCircuitSubsystem* CircuitSubsystem = AFGCircuitSubsystem::Get(World);
	if (!IsValid(CircuitSubsystem))
	{
		return;
	}

	TSet<UFGPowerConnectionComponent*>& Visited = OutVisited;
	Visited.Add(Seed);
	TArray<UFGPowerConnectionComponent*> Queue;
	Queue.Add(Seed);

	int32 EdgesPromoted = 0;
	constexpr int32 MaxEdgesPerFlood = 512;

	while (Queue.Num() > 0 && EdgesPromoted < MaxEdgesPerFlood)
	{
		UFGPowerConnectionComponent* Current = Queue[0];
		Queue.RemoveAt(0);

		TArray<UFGCircuitConnectionComponent*> HiddenLinks;
		Current->GetHiddenConnections(HiddenLinks);

		for (UFGCircuitConnectionComponent* OtherRaw : HiddenLinks)
		{
			UFGPowerConnectionComponent* Other = Cast<UFGPowerConnectionComponent>(OtherRaw);
			if (!IsValid(Other))
			{
				continue;
			}

			CircuitSubsystem->ConnectComponents(Current, Other);
			FStructuralPowerTrace::LogLinkOp(TEXT("mesh_promote"), Current, Other, true, TEXT("flood"));
			++EdgesPromoted;

			if (Other->IsHidden() && !Visited.Contains(Other))
			{
				Visited.Add(Other);
				Queue.Add(Other);
			}
		}
	}

	if (EdgesPromoted > 0)
	{
		UE_LOG(LogStructuralPower, Log,
			TEXT("[PWR] mesh flood from %s promoted %d edge(s) visited=%d"),
			*Seed->GetName(),
			EdgesPromoted,
			Visited.Num());

		RefreshBridgePolesOnMesh(Visited);
	}
}

void AStructuralPowerGraphSubsystem::OnLightweightRemoved(const FStructuralLightweightKey& Key)
{
	if (!Key.IsValid())
	{
		return;
	}

	const FStructuralNodeId NodeId = FStructuralLightweightIndex::MakeNodeId(Key);
	if (!RuntimeGraph.Contains(NodeId))
	{
		LightweightIndex.UnregisterMember(Key);
		PendingLightweightJobs.RemoveAll([&Key](const FStructuralLightweightKey& Pending)
		{
			return Pending == Key;
		});
		return;
	}

	if (UFGStructuralPowerConnectionComponent* SelfHidden = LightweightIndex.FindHiddenConnector(Key))
	{
		TArray<UFGCircuitConnectionComponent*> HiddenLinks;
		SelfHidden->GetHiddenConnections(HiddenLinks);
		for (UFGCircuitConnectionComponent* Other : HiddenLinks)
		{
			if (UFGPowerConnectionComponent* OtherPower = Cast<UFGPowerConnectionComponent>(Other))
			{
				SelfHidden->RemoveHiddenConnection(OtherPower);
			}
		}
	}

	RemoveGraphNode(NodeId);
	LightweightIndex.UnregisterMember(Key);
	PendingLightweightJobs.RemoveAll([&Key](const FStructuralLightweightKey& Pending)
	{
		return Pending == Key;
	});
}

void AStructuralPowerGraphSubsystem::OnBuildableRemoved(AFGBuildable* Buildable)
{
	if (!IsValid(Buildable))
	{
		return;
	}

	const FStructuralNodeId NodeId = MakeNodeId(Buildable);
	if (!RuntimeGraph.Contains(NodeId))
	{
		return;
	}

	UFGStructuralPowerConnectionComponent* SelfHidden = FindStructuralConnector(Buildable);
	if (!SelfHidden)
	{
		if (AFGBuildablePowerPole* Pole = Cast<AFGBuildablePowerPole>(Buildable))
		{
			SelfHidden = FindOutletBusConnector(Pole);
		}
	}

	if (SelfHidden)
	{
		TArray<UFGCircuitConnectionComponent*> HiddenLinks;
		SelfHidden->GetHiddenConnections(HiddenLinks);
		for (UFGCircuitConnectionComponent* Other : HiddenLinks)
		{
			if (UFGPowerConnectionComponent* OtherPower = Cast<UFGPowerConnectionComponent>(Other))
			{
				SelfHidden->RemoveHiddenConnection(OtherPower);
			}
		}
	}

	RemoveGraphNode(NodeId);

	PendingJobs.RemoveAll([&Buildable](const FDeferredPlacementJob& Job)
	{
		return Job.Buildable.Get() == Buildable;
	});
}

AFGBuildable* AStructuralPowerGraphSubsystem::ResolveBuildable(const FStructuralNodeId& NodeId) const
{
	if (!NodeId.IsValid() || NodeId.IsLightweight())
	{
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return nullptr;
	}

	if (AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(World))
	{
		for (AFGBuildable* Buildable : BuildableSubsystem->GetAllBuildablesRef())
		{
			if (MakeNodeId(Buildable) == NodeId)
			{
				return Buildable;
			}
		}
	}

	return nullptr;
}

FString AStructuralPowerGraphSubsystem::NodeDedupKey(const FStructuralNodeId& NodeId)
{
	return NodeId.BuildableClass.ToString()
		+ NodeId.ActorName.ToString()
		+ FString::FromInt(NodeId.LightweightIndex);
}

void AStructuralPowerGraphSubsystem::RegisterSavedLightweightNodes(UWorld* World)
{
	TArray<FStructuralLightweightKey> SavedKeys;
	SavedKeys.Reserve(RuntimeGraph.Num());

	for (const TPair<FStructuralNodeId, FStructuralNodeRecord>& Pair : RuntimeGraph)
	{
		if (Pair.Key.IsLightweight())
		{
			const FStructuralLightweightKey Key = FStructuralLightweightIndex::KeyFromNodeId(Pair.Key);
			if (Key.IsValid())
			{
				SavedKeys.Add(Key);
			}
		}
	}

	if (SavedKeys.Num() > 0)
	{
		LightweightIndex.RegisterSavedMembers(World, SavedKeys);
		LightweightIndex.RehydrateConnectorsFromSubsystem(World);
		UE_LOG(LogStructuralPower, Log, TEXT("Registered %d saved lightweight graph node(s)"), SavedKeys.Num());
	}
}

void AStructuralPowerGraphSubsystem::RebuildRuntimeFromSave()
{
	RuntimeGraph.Reset();

	for (const FStructuralNodeRecord& Saved : SavedGraph)
	{
		if (Saved.NodeId.IsValid())
		{
			RuntimeGraph.Add(Saved.NodeId, Saved);
		}
	}

	RegisterSavedLightweightNodes(GetWorld());

	int32 ComponentsCreated = 0;
	int32 LinksRestored = 0;

	for (const TPair<FStructuralNodeId, FStructuralNodeRecord>& Pair : RuntimeGraph)
	{
		UFGStructuralPowerConnectionComponent* Hidden = ResolveNodeConnector(Pair.Key, Pair.Value.bIsOutlet);
		if (!Hidden)
		{
			continue;
		}

		++ComponentsCreated;

		for (const FStructuralNodeId& NeighborId : Pair.Value.NeighborIds)
		{
			if (NodeDedupKey(Pair.Key) > NodeDedupKey(NeighborId))
			{
				continue;
			}

			if (const FStructuralNodeRecord* NeighborRecord = RuntimeGraph.Find(NeighborId))
			{
				UFGStructuralPowerConnectionComponent* NeighborHidden =
					ResolveNodeConnector(NeighborId, NeighborRecord->bIsOutlet);
				if (LinkHiddenPair(Hidden, NeighborHidden))
				{
					++LinksRestored;
				}
			}
		}
	}

	UE_LOG(LogStructuralPower, Log,
		TEXT("PostLoadGame graph rebuild: %d nodes, %d components, %d links restored"),
		RuntimeGraph.Num(),
		ComponentsCreated,
		LinksRestored);

	FStructuralPowerDiagnostics::AuditWorld(GetWorld());
}

void AStructuralPowerGraphSubsystem::SyncSaveFromRuntime()
{
	SavedGraph.Reset();
	SavedGraph.Reserve(RuntimeGraph.Num());
	for (const TPair<FStructuralNodeId, FStructuralNodeRecord>& Pair : RuntimeGraph)
	{
		SavedGraph.Add(Pair.Value);
	}
}

void AStructuralPowerGraphSubsystem::PreSaveGame_Implementation(int32 saveVersion, int32 gameVersion)
{
	SyncSaveFromRuntime();
}

void AStructuralPowerGraphSubsystem::PostLoadGame_Implementation(int32 saveVersion, int32 gameVersion)
{
	bPostLoadRebuilt = false;
}

void AStructuralPowerGraphSubsystem::RunDiagnostics() const
{
	FStructuralPowerDiagnostics::AuditWorld(GetWorld());
}
