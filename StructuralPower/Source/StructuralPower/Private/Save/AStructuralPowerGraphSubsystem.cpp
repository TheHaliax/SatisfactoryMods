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
#include "FGBuildableSubsystem.h"
#include "FGCircuitSubsystem.h"
#include "FGLightweightBuildableSubsystem.h"
#include "FGPowerConnectionComponent.h"
#include "Graph/FStructuralAdjacencyHeuristics.h"
#include "Graph/FStructuralOutletParentResolver.h"
#include "Kismet/GameplayStatics.h"
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

static void PromoteCircuitLink(
	UFGPowerConnectionComponent* A,
	UFGPowerConnectionComponent* B,
	ELogVerbosity::Type PromoteVerbosity = ELogVerbosity::Log)
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
			FStructuralPowerTrace::LogLinkOp(
				TEXT("promote"),
				A,
				B,
				true,
				TEXT("ConnectComponents"),
				PromoteVerbosity);
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
	if (!IsValid(World) || World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}

	if (AStructuralPowerGraphSubsystem* Existing = Cast<AStructuralPowerGraphSubsystem>(
		UGameplayStatics::GetActorOfClass(World, AStructuralPowerGraphSubsystem::StaticClass())))
	{
		return Existing;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = TEXT("StructuralPowerGraphSubsystem");
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	return World->SpawnActor<AStructuralPowerGraphSubsystem>(
		AStructuralPowerGraphSubsystem::StaticClass(),
		FTransform::Identity,
		SpawnParams);
}

AStructuralPowerGraphSubsystem* AStructuralPowerGraphSubsystem::Find(UWorld* World)
{
	if (!IsValid(World) || World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}

	return Cast<AStructuralPowerGraphSubsystem>(
		UGameplayStatics::GetActorOfClass(World, AStructuralPowerGraphSubsystem::StaticClass()));
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

UFGStructuralPowerConnectionComponent* AStructuralPowerGraphSubsystem::GetOrCreateOutletBusConnector(AFGBuildablePowerPole* Outlet)
{
	if (!IsValid(Outlet))
	{
		return nullptr;
	}

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
	return FStructuralOutletParentResolver::Resolve(Outlet, GetWorld(), LightweightIndex);
}

FStructuralNodeId AStructuralPowerGraphSubsystem::MakeParentNodeId(const FStructuralWallAnchor& Anchor)
{
	if (IsValid(Anchor.Actor))
	{
		return MakeNodeId(Anchor.Actor);
	}

	if (Anchor.Lightweight.IsValid())
	{
		return FStructuralLightweightIndex::MakeNodeId(Anchor.Lightweight);
	}

	return {};
}

void AStructuralPowerGraphSubsystem::OnWorldReady(UWorld* World)
{
	if (!IsValid(World) || World->GetNetMode() == NM_Client)
	{
		return;
	}

	if (bPostLoadRebuilt)
	{
		return;
	}

	RebuildBuildableRegistry(World);
	RebuildLightweightIndex(World);
	StructureGraph.Rebuild(World);
	PurgeSavedOutletBusMesh(World);
	bPostLoadRebuilt = true;

	// Seed every bridge pole through the normal deferred outlet path so its bus is created,
	// meshed to same-component sibling poles, and promoted if any pole is already wired. This
	// retroactively powers pre-existing bases: cost is bounded to the pole count and only
	// powered components ever touch the game circuit.
	int32 Seeded = 0;
	if (FStructuralPowerSessionSettings::IsPropagationEnabled())
	{
		if (AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(World))
		{
			for (AFGBuildable* Buildable : BuildableSubsystem->GetAllBuildablesRef())
			{
				if (IsValid(Buildable)
					&& Buildable->HasAuthority()
					&& FStructuralEligibilityRules::IsPowerBridgePole(Buildable))
				{
					EnqueuePlacement(Buildable, EStructuralPlacementJobType::Outlet, /*bDefer=*/true);
					++Seeded;
				}
			}
		}
	}

	int32 Components = 0;
	int32 Largest = 0;
	StructureGraph.GetComponentStats(Components, Largest);

	UE_LOG(LogStructuralPower, Log,
		TEXT("Graph ready — %d structure node(s), %d component(s) (largest %d), %d LW tracked, %d pole(s) seeded"),
		StructureGraph.GetNodeCount(),
		Components,
		Largest,
		LightweightIndex.GetTrackedCount(),
		Seeded);
}

void AStructuralPowerGraphSubsystem::PurgeSavedOutletBusMesh(UWorld* World)
{
	AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(World);
	if (!IsValid(BuildableSubsystem))
	{
		return;
	}

	AFGCircuitSubsystem* CircuitSubsystem = AFGCircuitSubsystem::Get(World);
	int32 Purged = 0;

	for (AFGBuildable* Buildable : BuildableSubsystem->GetAllBuildablesRef())
	{
		AFGBuildablePowerPole* Pole = Cast<AFGBuildablePowerPole>(Buildable);
		if (!IsValid(Pole) || !Pole->HasAuthority())
		{
			continue;
		}

		UFGStructuralPowerConnectionComponent* Bus = FindOutletBusConnector(Pole);
		if (!IsValid(Bus))
		{
			continue;
		}

		TArray<UFGCircuitConnectionComponent*> HiddenLinks;
		Bus->GetHiddenConnections(HiddenLinks);
		for (UFGCircuitConnectionComponent* Other : HiddenLinks)
		{
			// A saved hidden link may be recorded on only one endpoint, so clear both sides.
			if (IsValid(Other))
			{
				Other->RemoveHiddenConnection(Bus);
			}
			Bus->RemoveHiddenConnection(Other);
		}
		Bus->ClearHiddenConnections();

		if (IsValid(CircuitSubsystem))
		{
			CircuitSubsystem->RemoveComponent(Bus);
		}
		++Purged;
	}

	if (Purged > 0)
	{
		UE_LOG(LogStructuralPower, Log,
			TEXT("Purged saved outlet-bus mesh on %d pole(s) — rebuilding from geometry"),
			Purged);
	}
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
		if (IsLightweightAlreadyPending(Key))
		{
			return;
		}

		PendingLightweightJobs.Add(Key);
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
		if (IsBuildableAlreadyPending(Buildable, JobType))
		{
			return;
		}

		FDeferredPlacementJob Job;
		Job.Buildable = Buildable;
		Job.JobType = JobType;
		PendingJobs.Add(Job);
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

void AStructuralPowerGraphSubsystem::CompactPendingJobQueues()
{
	if (PendingJobsHead > 0)
	{
		PendingJobs.RemoveAt(0, PendingJobsHead, EAllowShrinking::No);
		PendingJobsHead = 0;
	}

	if (PendingLightweightJobsHead > 0)
	{
		PendingLightweightJobs.RemoveAt(0, PendingLightweightJobsHead, EAllowShrinking::No);
		PendingLightweightJobsHead = 0;
	}
}

bool AStructuralPowerGraphSubsystem::IsBuildableAlreadyPending(
	AFGBuildable* Buildable,
	EStructuralPlacementJobType JobType) const
{
	for (int32 Index = PendingJobsHead; Index < PendingJobs.Num(); ++Index)
	{
		const FDeferredPlacementJob& Job = PendingJobs[Index];
		if (Job.JobType == JobType && Job.Buildable.Get() == Buildable)
		{
			return true;
		}
	}

	return false;
}

bool AStructuralPowerGraphSubsystem::IsLightweightAlreadyPending(const FStructuralLightweightKey& Key) const
{
	for (int32 Index = PendingLightweightJobsHead; Index < PendingLightweightJobs.Num(); ++Index)
	{
		if (PendingLightweightJobs[Index] == Key)
		{
			return true;
		}
	}

	return false;
}

void AStructuralPowerGraphSubsystem::TickDeferredPlacements(int32 MaxJobs)
{
	if (!FStructuralPowerSessionSettings::IsPropagationEnabled() || MaxJobs <= 0)
	{
		return;
	}

	int32 Processed = 0;
	bool bPreferLightweight = true;

	while (Processed < MaxJobs)
	{
		const bool bHasLightweight = PendingLightweightJobsHead < PendingLightweightJobs.Num();
		const bool bHasActor = PendingJobsHead < PendingJobs.Num();

		if (!bHasLightweight && !bHasActor)
		{
			break;
		}

		if (bPreferLightweight && bHasLightweight)
		{
			const FStructuralLightweightKey Key = PendingLightweightJobs[PendingLightweightJobsHead++];
			ProcessLightweightStructure(Key);
			bPreferLightweight = !bPreferLightweight;
			++Processed;
			continue;
		}

		if (bHasActor)
		{
			const FDeferredPlacementJob Job = PendingJobs[PendingJobsHead++];
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

				bPreferLightweight = !bPreferLightweight;
				++Processed;
			}

			continue;
		}

		if (bHasLightweight)
		{
			const FStructuralLightweightKey Key = PendingLightweightJobs[PendingLightweightJobsHead++];
			ProcessLightweightStructure(Key);
			bPreferLightweight = !bPreferLightweight;
			++Processed;
			continue;
		}

		break;
	}

	if ((PendingJobsHead > 32 && PendingJobsHead * 2 >= PendingJobs.Num())
		|| (PendingLightweightJobsHead > 32 && PendingLightweightJobsHead * 2 >= PendingLightweightJobs.Num()))
	{
		CompactPendingJobQueues();
	}
}

bool AStructuralPowerGraphSubsystem::LinkHiddenPair(
	UFGPowerConnectionComponent* A,
	UFGPowerConnectionComponent* B)
{
	if (!IsValid(A) || !IsValid(B) || A == B)
	{
		return false;
	}

	if (A->HasHiddenConnection(B))
	{
		// Mesh already linked — still promote so newly wired generators merge circuit IDs.
		PromoteCircuitLink(A, B, ELogVerbosity::Verbose);
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
		return false;
	}

	FStructuralPowerTrace::LogLinkOp(TEXT("link"), A, B, bAdded, Path);

	if (bAdded)
	{
		PromoteCircuitLink(A, B);
	}

	return bAdded;
}

void AStructuralPowerGraphSubsystem::PromoteStructuralMeshFrom(UFGPowerConnectionComponent* Seed)
{
	if (!IsValid(Seed) || !ComponentCarriesPower(Seed))
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

	TSet<UFGPowerConnectionComponent*> Visited;
	Visited.Add(Seed);
	TArray<UFGPowerConnectionComponent*> Queue;
	Queue.Add(Seed);

	int32 EdgesPromoted = 0;
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

			CircuitSubsystem->ConnectComponents(Current, Other);
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
		UE_LOG(LogStructuralPower, Verbose,
			TEXT("[PWR] mesh flood from %s promoted %d edge(s) visited=%d"),
			*Seed->GetName(),
			EdgesPromoted,
			Visited.Num());
	}
}

UFGStructuralPowerConnectionComponent* AStructuralPowerGraphSubsystem::FindPoweredHiddenReachable(
	UFGStructuralPowerConnectionComponent* StartHidden,
	int32 MaxHiddenHops) const
{
	if (!IsValid(StartHidden))
	{
		return nullptr;
	}

	if (ComponentCarriesPower(StartHidden))
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
		if (ComponentCarriesPower(Current))
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

int32 AStructuralPowerGraphSubsystem::ResolvePoleComponentRoot(
	AFGBuildablePowerPole* Pole,
	const FStructuralWallAnchor& Anchor,
	FStructuralNodeId& OutParentId)
{
	OutParentId = FStructuralNodeId();
	if (!IsValid(Pole))
	{
		return INDEX_NONE;
	}

	if (Anchor.IsValid())
	{
		const FStructuralNodeId ParentId = MakeParentNodeId(Anchor);
		const int32 Root = StructureGraph.FindRoot(ParentId);
		if (Root != INDEX_NONE)
		{
			OutParentId = ParentId;
			return Root;
		}
	}

	// The resolver could not bind a tracked structure; fall back to the spatial index so the
	// pole still joins the nearest structural component and gets a stable parent reference.
	const FBox Bounds = FStructuralAdjacencyHeuristics::GetActorAdjacencyBounds(Pole);
	return StructureGraph.FindRootForBounds(Bounds, Pole->GetClass(), &OutParentId);
}

void AStructuralPowerGraphSubsystem::LinkBusToVisibleConnections(
	AFGBuildablePowerPole* Pole,
	UFGStructuralPowerConnectionComponent* Bus)
{
	if (!IsValid(Pole) || !IsValid(Bus))
	{
		return;
	}

	for (UFGPowerConnectionComponent* Visible : Pole->GetPowerConnections())
	{
		if (!IsValid(Visible) || Visible->IsHidden())
		{
			continue;
		}

		LinkHiddenPair(Bus, Visible);
	}
}

void AStructuralPowerGraphSubsystem::RestitchComponent(int32 Root, bool bTearDownFirst)
{
	if (Root == INDEX_NONE)
	{
		return;
	}

	TArray<AFGBuildablePowerPole*> Poles;
	TArray<UFGStructuralPowerConnectionComponent*> Buses;
	for (const TPair<FStructuralNodeId, FTrackedPole>& Pair : TrackedPoles)
	{
		AFGBuildablePowerPole* Pole = Pair.Value.Pole.Get();
		if (!IsValid(Pole))
		{
			continue;
		}

		if (StructureGraph.FindRoot(Pair.Value.ParentId) != Root)
		{
			continue;
		}

		UFGStructuralPowerConnectionComponent* Bus = GetOrCreateOutletBusConnector(Pole);
		if (!Bus)
		{
			continue;
		}

		Poles.Add(Pole);
		Buses.Add(Bus);
	}

	if (Buses.Num() == 0)
	{
		return;
	}

	if (bTearDownFirst)
	{
		// Drop only bus-to-bus (pole-to-pole) hidden links; keep each bus↔visible link so wired
		// poles never flicker. This severs any cross-component links left behind by a split.
		for (UFGStructuralPowerConnectionComponent* Bus : Buses)
		{
			TArray<UFGCircuitConnectionComponent*> HiddenLinks;
			Bus->GetHiddenConnections(HiddenLinks);
			for (UFGCircuitConnectionComponent* OtherRaw : HiddenLinks)
			{
				if (UFGStructuralPowerConnectionComponent* OtherBus =
					Cast<UFGStructuralPowerConnectionComponent>(OtherRaw))
				{
					Bus->RemoveHiddenConnection(OtherBus);
				}
			}
		}
	}

	UFGStructuralPowerConnectionComponent* Anchor = Buses[0];
	for (int32 Index = 0; Index < Buses.Num(); ++Index)
	{
		LinkBusToVisibleConnections(Poles[Index], Buses[Index]);
		if (Index > 0)
		{
			LinkHiddenPair(Buses[Index], Anchor);
		}
	}

	// If any pole in the component is powered, flood-promote the whole bus mesh so every
	// sibling pole (and its machines) shares the circuit.
	UFGStructuralPowerConnectionComponent* Seed = nullptr;
	for (UFGStructuralPowerConnectionComponent* Bus : Buses)
	{
		if (ComponentCarriesPower(Bus))
		{
			Seed = Bus;
			break;
		}
	}
	if (!Seed)
	{
		for (UFGStructuralPowerConnectionComponent* Bus : Buses)
		{
			if (UFGStructuralPowerConnectionComponent* Reachable = FindPoweredHiddenReachable(Bus))
			{
				Seed = Reachable;
				break;
			}
		}
	}
	if (Seed)
	{
		PromoteStructuralMeshFrom(Seed);
	}
}

void AStructuralPowerGraphSubsystem::ReEnergizeComponentRoots(const TArray<int32>& Roots, bool bTearDownFirst)
{
	TSet<int32> Done;
	for (int32 Root : Roots)
	{
		if (Root == INDEX_NONE || Done.Contains(Root))
		{
			continue;
		}
		Done.Add(Root);
		RestitchComponent(Root, bTearDownFirst);
	}
}

void AStructuralPowerGraphSubsystem::ProcessStructure(AFGBuildable* Buildable)
{
	if (!FStructuralEligibilityRules::IsBusMember(Buildable))
	{
		FStructuralPowerTrace::LogPlacementSkip(Buildable, TEXT("not_bus_member"));
		return;
	}

	RegisterBuildableActor(Buildable);

	TArray<int32> MergedRoots;
	if (!StructureGraph.AddActorNode(Buildable, MergedRoots))
	{
		return;
	}

	// Only a genuine fusion of two+ previously separate components can change which poles share
	// power. Extending a single component (or an isolated add) leaves pole grouping untouched.
	if (MergedRoots.Num() >= 2)
	{
		const int32 Root = StructureGraph.FindRoot(MakeNodeId(Buildable));
		TArray<int32> Roots;
		Roots.Add(Root);
		ReEnergizeComponentRoots(Roots, /*bTearDownFirst=*/false);

		UE_LOG(LogStructuralPower, Verbose,
			TEXT("[PWR] structure %s fused %d component(s) -> root %d"),
			*Buildable->GetName(),
			MergedRoots.Num(),
			Root);
	}
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

	LightweightIndex.RegisterTrackedMember(World, Key);

	TArray<int32> MergedRoots;
	if (!StructureGraph.AddLightweightNode(World, Key, MergedRoots))
	{
		return;
	}

	if (MergedRoots.Num() >= 2)
	{
		const int32 Root = StructureGraph.FindRoot(FStructuralLightweightIndex::MakeNodeId(Key));
		TArray<int32> Roots;
		Roots.Add(Root);
		ReEnergizeComponentRoots(Roots, /*bTearDownFirst=*/false);
	}
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

	const FStructuralWallAnchor ParentAnchor = ResolveOutletAnchor(Buildable);
	FStructuralNodeId ParentId;
	const int32 Root = ResolvePoleComponentRoot(Pole, ParentAnchor, ParentId);

	const FStructuralNodeId PoleId = MakeNodeId(Buildable);
	FTrackedPole& Tracked = TrackedPoles.FindOrAdd(PoleId);
	Tracked.Pole = Pole;
	Tracked.ParentId = ParentId;
	RegisterBuildableActor(Buildable);

	LinkBusToVisibleConnections(Pole, OutletBus);

	// Mesh this pole into any sibling pole already tracked in the same structural component.
	// Each pole links to one already-meshed sibling, so the component's buses form one
	// connected mesh regardless of processing order.
	if (Root != INDEX_NONE)
	{
		for (const TPair<FStructuralNodeId, FTrackedPole>& Pair : TrackedPoles)
		{
			if (Pair.Key == PoleId)
			{
				continue;
			}

			AFGBuildablePowerPole* Sibling = Pair.Value.Pole.Get();
			if (!IsValid(Sibling) || StructureGraph.FindRoot(Pair.Value.ParentId) != Root)
			{
				continue;
			}

			if (UFGStructuralPowerConnectionComponent* SiblingBus = FindOutletBusConnector(Sibling))
			{
				LinkHiddenPair(OutletBus, SiblingBus);
				break;
			}
		}
	}

	UFGStructuralPowerConnectionComponent* Seed = ComponentCarriesPower(OutletBus)
		? OutletBus
		: FindPoweredHiddenReachable(OutletBus);
	if (Seed)
	{
		PromoteStructuralMeshFrom(Seed);
	}

	UE_LOG(LogStructuralPower, Log,
		TEXT("[PWR] outlet %s root=%d parentValid=%d busCircuit=%d powered=%d"),
		*Buildable->GetName(),
		Root,
		ParentAnchor.IsValid() ? 1 : 0,
		OutletBus->GetCircuitID(),
		ComponentCarriesPower(OutletBus) ? 1 : 0);
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

void AStructuralPowerGraphSubsystem::OnBuildableRemoved(AFGBuildable* Buildable)
{
	if (!IsValid(Buildable))
	{
		return;
	}

	const FStructuralNodeId NodeId = MakeNodeId(Buildable);
	UnregisterBuildableActor(NodeId);

	if (FTrackedPole* Tracked = TrackedPoles.Find(NodeId))
	{
		const int32 OldRoot = StructureGraph.FindRoot(Tracked->ParentId);

		if (AFGBuildablePowerPole* Pole = Cast<AFGBuildablePowerPole>(Buildable))
		{
			if (UFGStructuralPowerConnectionComponent* Bus = FindOutletBusConnector(Pole))
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

		TrackedPoles.Remove(NodeId);

		if (OldRoot != INDEX_NONE)
		{
			TArray<int32> Roots;
			Roots.Add(OldRoot);
			ReEnergizeComponentRoots(Roots, /*bTearDownFirst=*/true);
		}
	}
	else if (StructureGraph.IsTracked(NodeId))
	{
		TArray<int32> AffectedRoots;
		StructureGraph.RemoveNode(NodeId, AffectedRoots);
		ReEnergizeComponentRoots(AffectedRoots, /*bTearDownFirst=*/true);
	}

	CompactPendingJobQueues();
	PendingJobs.RemoveAll([&Buildable](const FDeferredPlacementJob& Job)
	{
		return Job.Buildable.Get() == Buildable;
	});
}

void AStructuralPowerGraphSubsystem::OnLightweightRemoved(const FStructuralLightweightKey& Key)
{
	if (!Key.IsValid())
	{
		return;
	}

	const FStructuralNodeId NodeId = FStructuralLightweightIndex::MakeNodeId(Key);
	if (StructureGraph.IsTracked(NodeId))
	{
		TArray<int32> AffectedRoots;
		StructureGraph.RemoveNode(NodeId, AffectedRoots);
		ReEnergizeComponentRoots(AffectedRoots, /*bTearDownFirst=*/true);
	}

	LightweightIndex.UnregisterMember(Key);

	CompactPendingJobQueues();
	PendingLightweightJobs.RemoveAll([&Key](const FStructuralLightweightKey& Pending)
	{
		return Pending == Key;
	});
}

void AStructuralPowerGraphSubsystem::RegisterBuildableActor(AFGBuildable* Buildable)
{
	if (!IsValid(Buildable))
	{
		return;
	}

	RegisteredBuildables.Add(MakeNodeId(Buildable), Buildable);
}

void AStructuralPowerGraphSubsystem::UnregisterBuildableActor(const FStructuralNodeId& NodeId)
{
	if (!NodeId.IsValid() || NodeId.IsLightweight())
	{
		return;
	}

	RegisteredBuildables.Remove(NodeId);
}

void AStructuralPowerGraphSubsystem::RebuildBuildableRegistry(UWorld* World)
{
	RegisteredBuildables.Reset();

	if (!IsValid(World))
	{
		return;
	}

	AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(World);
	if (!IsValid(BuildableSubsystem))
	{
		return;
	}

	for (AFGBuildable* Buildable : BuildableSubsystem->GetAllBuildablesRef())
	{
		if (!IsValid(Buildable))
		{
			continue;
		}

		if (FStructuralEligibilityRules::IsBusMember(Buildable)
			|| FStructuralEligibilityRules::IsPowerBridgePole(Buildable))
		{
			RegisteredBuildables.Add(MakeNodeId(Buildable), Buildable);
		}
	}
}

void AStructuralPowerGraphSubsystem::RebuildLightweightIndex(UWorld* World)
{
	if (!IsValid(World))
	{
		return;
	}

	AFGLightweightBuildableSubsystem* Lw = AFGLightweightBuildableSubsystem::Get(World);
	if (!IsValid(Lw))
	{
		return;
	}

	for (const TPair<TSubclassOf<AFGBuildable>, TArray<FRuntimeBuildableInstanceData>>& Pair :
		Lw->GetAllLightweightBuildableInstances())
	{
		TSubclassOf<AFGBuildable> Class = Pair.Key;
		if (!Class)
		{
			continue;
		}

		const AFGBuildable* CDO = Class->GetDefaultObject<AFGBuildable>();
		if (!FStructuralEligibilityRules::IsBusMember(CDO))
		{
			continue;
		}

		const TArray<FRuntimeBuildableInstanceData>& Instances = Pair.Value;
		for (int32 Index = 0; Index < Instances.Num(); ++Index)
		{
			if (Instances[Index].IsValidOnLoad())
			{
				LightweightIndex.RegisterTrackedMember(World, FStructuralLightweightKey{Class, Index});
			}
		}
	}
}

void AStructuralPowerGraphSubsystem::RunDiagnostics() const
{
	FStructuralPowerDiagnostics::AuditWorld(GetWorld(), true);
}
