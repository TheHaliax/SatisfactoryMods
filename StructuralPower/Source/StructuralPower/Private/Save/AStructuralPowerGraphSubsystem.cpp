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
#include "Graph/FOverlapBoxAdjacencyStrategy.h"
#include "Graph/FStructuralAdjacencyHeuristics.h"
#include "Graph/FStructuralOutletParentHeuristics.h"
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

static FString FormatLinkedNeighborNames(const TArray<AFGBuildable*>& Neighbors, int32 MaxNames = 4)
{
	FString Result;
	for (int32 Index = 0; Index < Neighbors.Num() && Index < MaxNames; ++Index)
	{
		if (IsValid(Neighbors[Index]))
		{
			if (!Result.IsEmpty())
			{
				Result += TEXT(", ");
			}

			Result += Neighbors[Index]->GetName();
		}
	}

	if (Neighbors.Num() > MaxNames)
	{
		Result += FString::Printf(TEXT(" +%d more"), Neighbors.Num() - MaxNames);
	}

	return Result;
}

static void LogBeamStitchMiss(
	UWorld* World,
	AFGBuildable* Buildable,
	UFGStructuralPowerConnectionComponent* SelfHidden,
	int32 LinksCreated)
{
	if (!IsValid(Buildable) || LinksCreated > 0 || !FStructuralAdjacencyHeuristics::IsBeamBuildable(Buildable))
	{
		return;
	}

	if (IsValid(SelfHidden))
	{
		TArray<UFGCircuitConnectionComponent*> HiddenLinks;
		SelfHidden->GetHiddenConnections(HiddenLinks);
		if (HiddenLinks.Num() > 0)
		{
			return;
		}
	}

	AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(World);
	if (!IsValid(BuildableSubsystem))
	{
		return;
	}

	const FBox SourceBounds = FStructuralAdjacencyHeuristics::GetActorAdjacencyBounds(Buildable);
	float BestDistCm = TNumericLimits<float>::Max();
	FString BestName = TEXT("none");

	for (AFGBuildable* Candidate : BuildableSubsystem->GetAllBuildablesRef())
	{
		if (!IsValid(Candidate) || Candidate == Buildable || !FStructuralEligibilityRules::IsBusMember(Candidate))
		{
			continue;
		}

		const float DistCm = FStructuralAdjacencyHeuristics::ComputeAdjacencyBoundsDistCm(
			SourceBounds,
			FStructuralAdjacencyHeuristics::GetActorAdjacencyBounds(Candidate));
		if (DistCm < BestDistCm)
		{
			BestDistCm = DistCm;
			BestName = Candidate->GetName();
		}
	}

	UE_LOG(LogStructuralPower, Warning,
		TEXT("[PWR] beam stitch miss %s nearest=%s distCm=%.1f maxGapCm=%.0f"),
		*Buildable->GetName(),
		*BestName,
		BestDistCm,
		StructuralPowerConstants::BeamStructuralGapCm);
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

static bool EnsureHiddenLink(
	UFGPowerConnectionComponent* A,
	UFGPowerConnectionComponent* B,
	TFunctionRef<bool(UFGPowerConnectionComponent*, UFGPowerConnectionComponent*)> LinkFn)
{
	if (!IsValid(A) || !IsValid(B))
	{
		return false;
	}

	if (A->HasHiddenConnection(B))
	{
		return true;
	}

	return LinkFn(A, B);
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

	if (World->GetNetMode() == NM_Client)
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

bool AStructuralPowerGraphSubsystem::HasStructuralConnector(const AFGBuildable* Buildable)
{
	if (const AFGBuildablePowerPole* Pole = Cast<AFGBuildablePowerPole>(Buildable))
	{
		return FindStructuralConnector(Buildable) != nullptr
			|| FindOutletBusConnector(Pole) != nullptr;
	}

	return FindStructuralConnector(Buildable) != nullptr;
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

	RebuildBuildableRegistry(World);

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
		if (IsLightweightAlreadyPending(Key))
		{
			return;
		}

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
		if (IsBuildableAlreadyPending(Buildable, JobType))
		{
			FStructuralPowerTrace::LogHook(
				Buildable,
				TEXT("EnqueuePlacement"),
				JobType == EStructuralPlacementJobType::Outlet ? TEXT("skipped_outlet") : TEXT("skipped_structure"),
				TEXT("already_pending"));
			return;
		}

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

	FlushPendingBridgePoleRefresh();
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
	return FStructuralOutletParentResolver::Resolve(Outlet, GetWorld(), LightweightIndex);
}

const FStructuralNodeRecord* AStructuralPowerGraphSubsystem::FindRuntimeNodeRecord(
	const FStructuralNodeId& NodeId) const
{
	return RuntimeGraph.Find(NodeId);
}

FStructuralWallAnchor AStructuralPowerGraphSubsystem::AnchorFromRuntimeNodeId(
	const FStructuralNodeId& NodeId) const
{
	FStructuralWallAnchor Anchor;
	if (!NodeId.IsValid())
	{
		return Anchor;
	}

	if (NodeId.IsLightweight())
	{
		Anchor.Lightweight = FStructuralLightweightIndex::KeyFromNodeId(NodeId);
		if (const FStructuralNodeRecord* Record = RuntimeGraph.Find(NodeId))
		{
			Anchor.WorldLocation = Record->WorldLocation;
		}
	}
	else if (AFGBuildable* Buildable = const_cast<AStructuralPowerGraphSubsystem*>(this)->ResolveBuildable(NodeId))
	{
		Anchor.Actor = Buildable;
		Anchor.WorldLocation = Buildable->GetActorLocation();
	}

	return Anchor;
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
		// Mesh already linked — still promote so newly wired generators merge circuit IDs.
		PromoteCircuitLink(A, B, ELogVerbosity::Verbose);
		FStructuralPowerTrace::LogLinkOp(
			TEXT("link"),
			A,
			B,
			false,
			TEXT("already_linked"),
			ELogVerbosity::Verbose);
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

	RegisterBuildableActor(Buildable);

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

	TArray<AFGBuildable*> LinkedNeighbors;
	int32 LinksCreated = 0;
	for (AFGBuildable* Neighbor : Neighbors)
	{
		UFGStructuralPowerConnectionComponent* NeighborHidden = GetOrCreateStructuralConnector(Neighbor);
		if (!NeighborHidden)
		{
			continue;
		}

		if (EnsureHiddenLink(SelfHidden, NeighborHidden, [this](UFGPowerConnectionComponent* A, UFGPowerConnectionComponent* B)
		{
			return LinkHiddenPair(A, B);
		}))
		{
			RegisterGraphEdge(SelfId, MakeNodeId(Neighbor), false, false);
			LinkedNeighbors.Add(Neighbor);
			++LinksCreated;
		}
	}

	const FBox ActorBounds = FStructuralAdjacencyHeuristics::GetActorAdjacencyBounds(Buildable);

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

		if (EnsureHiddenLink(SelfHidden, NeighborHidden, [this](UFGPowerConnectionComponent* A, UFGPowerConnectionComponent* B)
		{
			return LinkHiddenPair(A, B);
		}))
		{
			RegisterGraphEdge(SelfId, FStructuralLightweightIndex::MakeNodeId(NeighborKey), false, false);
			++LinksCreated;
		}
	}

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

	LogBeamStitchMiss(GetWorld(), Buildable, SelfHidden, LinksCreated);

	UE_LOG(LogStructuralPower, Log,
		TEXT("[PWR] structure %s linked %d mod neighbor(s) circuitId=%d neighbors=[%s]"),
		*Buildable->GetName(),
		LinksCreated,
		SelfHidden->GetCircuitID(),
		*FormatLinkedNeighborNames(LinkedNeighbors));
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
	FBox MemberBounds(ForceInit);
	if (AFGLightweightBuildableSubsystem* LwSubsystem = AFGLightweightBuildableSubsystem::Get(World))
	{
		if (const TArray<FRuntimeBuildableInstanceData>* Instances =
			LwSubsystem->GetAllLightweightBuildableInstances().Find(Key.BuildableClass))
		{
			if (Instances->IsValidIndex(Key.Index))
			{
				const FRuntimeBuildableInstanceData& Data = (*Instances)[Key.Index];
				MemberBounds = Data.BoundingBox.IsValid
					? Data.BoundingBox.TransformBy(Data.Transform)
					: FBox(Data.Transform.GetLocation(), Data.Transform.GetLocation());
				if (!MemberBounds.IsValid)
				{
					const float Extent = StructuralPowerConstants::DefaultFoundationExtentCm;
					const FVector HalfExtents(Extent, Extent, Extent);
					MemberBounds = FBox(Data.Transform.GetLocation() - HalfExtents, Data.Transform.GetLocation() + HalfExtents);
				}

				ConnectorLocation = MemberBounds.GetCenter();
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

	int32 LinksCreated = LightweightIndex.StitchPlacementNeighbors(
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

	if (MemberBounds.IsValid)
	{
		FBox SearchBounds = FStructuralOutletParentHeuristics::ExpandBoundsForClass(
			MemberBounds,
			Key.BuildableClass);
		if (FStructuralAdjacencyHeuristics::IsBeamClass(Key.BuildableClass))
		{
			SearchBounds = SearchBounds.ExpandBy(StructuralPowerConstants::BeamOverlapPaddingCm);
		}

		TArray<AFGBuildable*> ActorNeighbors;
		FOverlapBoxAdjacencyStrategy::FindBusNeighborsNearBounds(
			World,
			SearchBounds,
			Key.BuildableClass,
			nullptr,
			ActorNeighbors);

		int32 ActorLinks = 0;
		for (AFGBuildable* ActorNeighbor : ActorNeighbors)
		{
			UFGStructuralPowerConnectionComponent* NeighborHidden = GetOrCreateStructuralConnector(ActorNeighbor);
			if (!NeighborHidden)
			{
				continue;
			}

			if (LinkHiddenPair(SelfHidden, NeighborHidden))
			{
				RegisterGraphEdge(SelfId, MakeNodeId(ActorNeighbor), false, false);
				++LinksCreated;
				++ActorLinks;
			}
		}

		if (ActorLinks > 0)
		{
			UE_LOG(LogStructuralPower, Log,
				TEXT("[PWR] LW %s[%d] actor-bridge linked %d actor neighbor(s)"),
				*Key.BuildableClass->GetName(),
				Key.Index,
				ActorLinks);
		}
	}

	if (LinksCreated == 0)
	{
		UE_LOG(LogStructuralPower, Warning,
			TEXT("[PWR] LW stitch miss %s[%d] boundsValid=%d tracked=%d"),
			*Key.BuildableClass->GetName(),
			Key.Index,
			MemberBounds.IsValid ? 1 : 0,
			LightweightIndex.GetTrackedCount());
	}

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
		// Hidden mesh is connected; one powered neighbor seeds full mesh promotion.
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

void AStructuralPowerGraphSubsystem::ClearStaleOutletParentBinding(
	AFGBuildablePowerPole* Pole,
	FStructuralNodeRecord& OutletRecord)
{
	if (!OutletRecord.ParentNodeId.IsValid())
	{
		return;
	}

	UFGStructuralPowerConnectionComponent* OutletBus = FindOutletBusConnector(Pole);
	if (UFGStructuralPowerConnectionComponent* OldParentHidden =
		Cast<UFGStructuralPowerConnectionComponent>(ResolveNodeConnector(OutletRecord.ParentNodeId, false)))
	{
		if (IsValid(OutletBus) && OutletBus->HasHiddenConnection(OldParentHidden))
		{
			OutletBus->RemoveHiddenConnection(OldParentHidden);
		}
	}

	OutletRecord.ParentNodeId = {};
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
	const FStructuralWallAnchor ParentAnchor = ResolveOutletAnchor(Buildable);
	const FStructuralNodeId OutletId = MakeNodeId(Buildable);
	FStructuralNodeRecord& OutletRecord = RuntimeGraph.FindOrAdd(OutletId);
	OutletRecord.NodeId = OutletId;
	OutletRecord.bIsOutlet = true;
	OutletRecord.WorldLocation = Buildable->GetActorLocation();

	if (!ParentAnchor.IsValid())
	{
		const FString ParentName = IsValid(ParentActor) ? ParentActor->GetName() : TEXT("null");
		const float NearestDistSq = FStructuralOutletParentHeuristics::FindNearestBusDistSq(Buildable, GetWorld());
		const float NearestDistCm = FMath::Sqrt(NearestDistSq);
		ClearStaleOutletParentBinding(Pole, OutletRecord);
		UE_LOG(LogStructuralPower, Warning,
			TEXT("[PWR] outlet %s NO_PARENT parent=%s trackedLW=%d nearestDistCm=%.1f maxDistCm=%.1f"),
			*Buildable->GetName(),
			*ParentName,
			LightweightIndex.GetTrackedCount(),
			NearestDistCm,
			StructuralPowerConstants::MaxOutletParentDistCm);
		FStructuralPowerTrace::LogOutletBridge(Buildable, false, INDEX_NONE, OutletBus->GetCircuitID(), 0, TEXT("no_mod_parent"));
		return;
	}

	if (ParentAnchor.Lightweight.IsValid())
	{
		if (!LightweightIndex.IsTracked(ParentAnchor.Lightweight))
		{
			ProcessLightweightStructure(ParentAnchor.Lightweight);
		}
	}
	else if (IsValid(ParentAnchor.Actor) && !HasStructuralConnector(ParentAnchor.Actor))
	{
		ProcessStructure(ParentAnchor.Actor);
	}

	UFGStructuralPowerConnectionComponent* ParentHidden = GetOrCreateStructureHidden(ParentAnchor);
	if (!ParentHidden)
	{
		FStructuralPowerTrace::LogOutletBridge(Buildable, false, INDEX_NONE, OutletBus->GetCircuitID(), 0, TEXT("parent_hidden_failed"));
		return;
	}

	const FStructuralNodeId ParentId = MakeParentNodeId(ParentAnchor);

	if (OutletRecord.ParentNodeId.IsValid() && OutletRecord.ParentNodeId != ParentId)
	{
		ClearStaleOutletParentBinding(Pole, OutletRecord);
	}

	OutletRecord.ParentNodeId = ParentId;

	const bool bStructLinked = EnsureHiddenLink(OutletBus, ParentHidden, [this](UFGPowerConnectionComponent* A, UFGPowerConnectionComponent* B)
	{
		return LinkHiddenPair(A, B);
	});
	if (bStructLinked)
	{
		RegisterGraphEdge(OutletId, ParentId, true, false);
	}

	RegisterBuildableActor(Buildable);

	TryEnergizeParentMesh(ParentHidden, /*bAlwaysFlood=*/false);
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
		if (UFGStructuralPowerConnectionComponent* FloodSeed =
			TryEnergizeParentMesh(ParentHidden, /*bAlwaysFlood=*/true))
		{
			PromoteCircuitLink(OutletBus, FloodSeed);
		}
	}

	const bool bStructConnected = bStructLinked || OutletBus->HasHiddenConnection(ParentHidden);
	FStructuralPowerTrace::LogOutletBridge(
		Buildable,
		bStructConnected || VisibleLinks > 0,
		ParentHidden->GetCircuitID(),
		OutletBus->GetCircuitID(),
		WiredVisible,
		bStructConnected ? TEXT("ok") : TEXT("struct_link_failed"));

	UE_LOG(LogStructuralPower, Log,
		TEXT("[PWR] outlet %s parent=%s structLink=%d visibleLinks=%d busCircuit=%d parentCircuit=%d"),
		*Buildable->GetName(),
		IsValid(ParentAnchor.Actor) ? *ParentAnchor.Actor->GetName()
			: *FString::Printf(TEXT("LW:%s[%d]"), *ParentAnchor.Lightweight.BuildableClass->GetName(), ParentAnchor.Lightweight.Index),
		bStructConnected ? 1 : 0,
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

UFGStructuralPowerConnectionComponent* AStructuralPowerGraphSubsystem::TryEnergizeParentMesh(
	UFGStructuralPowerConnectionComponent* ParentHidden,
	bool bAlwaysFlood)
{
	if (!IsValid(ParentHidden))
	{
		return nullptr;
	}

	UFGStructuralPowerConnectionComponent* PoweredParentHidden = nullptr;
	if (ComponentCarriesPower(ParentHidden))
	{
		PoweredParentHidden = ParentHidden;
	}
	else
	{
		PoweredParentHidden = FindPoweredHiddenReachable(
			ParentHidden,
			StructuralPowerConstants::PassiveEnergizeMaxHiddenHops);
	}

	if (!IsValid(PoweredParentHidden))
	{
		return nullptr;
	}

	const bool bShouldFlood = bAlwaysFlood
		|| (!ComponentCarriesPower(ParentHidden) && PoweredParentHidden != ParentHidden);

	if (bShouldFlood)
	{
		TSet<UFGPowerConnectionComponent*> PoweredNodes;
		PromoteStructuralMeshFrom(PoweredParentHidden, PoweredNodes);
	}

	return PoweredParentHidden;
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

void AStructuralPowerGraphSubsystem::SyncBridgePoleToPoweredParent(
	AFGBuildablePowerPole* Pole,
	UFGStructuralPowerConnectionComponent* OutletBus,
	UFGStructuralPowerConnectionComponent* ParentHidden)
{
	if (!IsValid(Pole) || !IsValid(OutletBus) || !IsValid(ParentHidden))
	{
		return;
	}

	if (!ComponentCarriesPower(ParentHidden))
	{
		return;
	}

	PromoteCircuitLink(OutletBus, ParentHidden);

	int32 VisibleSynced = 0;
	const bool bParentPowered = ComponentCarriesPower(ParentHidden);
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
		else if (OutletBus->HasHiddenConnection(Visible) || bParentPowered)
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

void AStructuralPowerGraphSubsystem::QueueBridgePoleRefresh(
	const TSet<UFGPowerConnectionComponent*>& PoweredHiddenNodes)
{
	if (PoweredHiddenNodes.Num() == 0)
	{
		return;
	}

	PendingPoweredMeshRefresh.Append(PoweredHiddenNodes);
}

void AStructuralPowerGraphSubsystem::FlushPendingBridgePoleRefresh()
{
	if (PendingPoweredMeshRefresh.Num() == 0)
	{
		return;
	}

	const TSet<UFGPowerConnectionComponent*> NodesToRefresh = MoveTemp(PendingPoweredMeshRefresh);
	PendingPoweredMeshRefresh.Reset();
	RefreshBridgePolesOnMesh(NodesToRefresh);
}

void AStructuralPowerGraphSubsystem::RefreshBridgePolesOnMesh(
	const TSet<UFGPowerConnectionComponent*>& PoweredHiddenNodes)
{
	if (PoweredHiddenNodes.Num() == 0)
	{
		return;
	}

	int32 Refreshed = 0;
	for (const TPair<FStructuralNodeId, FStructuralNodeRecord>& Pair : RuntimeGraph)
	{
		const FStructuralNodeRecord& Record = Pair.Value;
		if (!Record.bIsOutlet || !Record.ParentNodeId.IsValid())
		{
			continue;
		}

		UFGStructuralPowerConnectionComponent* ParentHidden =
			Cast<UFGStructuralPowerConnectionComponent>(ResolveNodeConnector(Record.ParentNodeId, false));
		if (!IsValid(ParentHidden) || !PoweredHiddenNodes.Contains(ParentHidden))
		{
			continue;
		}

		AFGBuildable* Buildable = ResolveBuildable(Pair.Key);
		AFGBuildablePowerPole* Pole = Cast<AFGBuildablePowerPole>(Buildable);
		UFGStructuralPowerConnectionComponent* OutletBus = FindOutletBusConnector(Pole);
		if (!IsValid(Pole) || !OutletBus)
		{
			continue;
		}

		const FStructuralWallAnchor CurrentParent = ResolveOutletAnchor(Pole);
		if (!CurrentParent.IsValid())
		{
			continue;
		}

		const FStructuralNodeId CurrentParentId = MakeParentNodeId(CurrentParent);
		if (CurrentParentId != Record.ParentNodeId)
		{
			continue;
		}

		if (!ComponentCarriesPower(ParentHidden))
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
		const int32 TotalHiddenNodes = CountStructuralHiddenNodes();
		const int32 Unvisited = FMath::Max(0, TotalHiddenNodes - Visited.Num());
		UE_LOG(LogStructuralPower, Log,
			TEXT("[PWR] mesh flood from %s promoted %d edge(s) visited=%d unvisited=%d totalHidden=%d"),
			*Seed->GetName(),
			EdgesPromoted,
			Visited.Num(),
			Unvisited,
			TotalHiddenNodes);

		QueueBridgePoleRefresh(Visited);
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
		CompactPendingJobQueues();
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
	CompactPendingJobQueues();
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
	UnregisterBuildableActor(NodeId);

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

	CompactPendingJobQueues();
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

	if (const TWeakObjectPtr<AFGBuildable>* Found = RegisteredBuildables.Find(NodeId))
	{
		return Found->Get();
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
	RebuildBuildableRegistry(GetWorld());

	// Drop stale cross-gap hidden links baked into older saves before re-linking/energizing.
	ValidateSavedHiddenLinks();

	int32 ComponentsCreated = 0;
	int32 LinksNewlyAdded = 0;
	int32 LinksAlreadyPresent = 0;

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
				if (Hidden->HasHiddenConnection(NeighborHidden))
				{
					LinkHiddenPair(Hidden, NeighborHidden);
					++LinksAlreadyPresent;
				}
				else if (LinkHiddenPair(Hidden, NeighborHidden))
				{
					++LinksNewlyAdded;
				}
			}
		}
	}

	UE_LOG(LogStructuralPower, Log,
		TEXT("PostLoadGame graph rebuild: %d nodes, %d components, %d links newly added, %d already present"),
		RuntimeGraph.Num(),
		ComponentsCreated,
		LinksNewlyAdded,
		LinksAlreadyPresent);

	FStructuralPowerDiagnostics::AuditWorld(GetWorld());
}

bool AStructuralPowerGraphSubsystem::GetNodeAdjacencyInfo(
	const FStructuralNodeId& NodeId,
	FBox& OutBounds,
	TSubclassOf<AFGBuildable>& OutClass) const
{
	if (NodeId.IsLightweight())
	{
		const FStructuralLightweightKey Key = FStructuralLightweightIndex::KeyFromNodeId(NodeId);
		FBox Bounds(ForceInit);
		if (Key.IsValid() && LightweightIndex.GetMemberBounds(Key, Bounds))
		{
			OutBounds = Bounds;
			OutClass = Key.BuildableClass;
			return true;
		}
		return false;
	}

	if (AFGBuildable* Buildable = ResolveBuildable(NodeId))
	{
		OutBounds = FStructuralAdjacencyHeuristics::GetActorAdjacencyBounds(Buildable);
		OutClass = Buildable->GetClass();
		return OutBounds.IsValid != 0;
	}

	return false;
}

void AStructuralPowerGraphSubsystem::ValidateSavedHiddenLinks()
{
	// Structure-structure hidden links persisted by older builds can bridge foundations
	// that are physically far apart (the "wireless" megamesh). Re-check each saved edge
	// against the same structural-adjacency predicate placement uses and drop the ones
	// that no longer connect. Outlet/parent edges are left to the resolver; only clearly
	// non-adjacent structure pairs are cut so contiguous meshes stay intact.
	TArray<TPair<FStructuralNodeId, FStructuralNodeId>> ToPrune;
	int32 Checked = 0;

	for (const TPair<FStructuralNodeId, FStructuralNodeRecord>& Pair : RuntimeGraph)
	{
		if (Pair.Value.bIsOutlet)
		{
			continue;
		}

		FBox BoundsA(ForceInit);
		TSubclassOf<AFGBuildable> ClassA = nullptr;
		if (!GetNodeAdjacencyInfo(Pair.Key, BoundsA, ClassA))
		{
			continue;
		}

		for (const FStructuralNodeId& NeighborId : Pair.Value.NeighborIds)
		{
			if (NodeDedupKey(Pair.Key) > NodeDedupKey(NeighborId))
			{
				continue;
			}

			const FStructuralNodeRecord* NeighborRecord = RuntimeGraph.Find(NeighborId);
			if (!NeighborRecord || NeighborRecord->bIsOutlet)
			{
				continue;
			}

			FBox BoundsB(ForceInit);
			TSubclassOf<AFGBuildable> ClassB = nullptr;
			if (!GetNodeAdjacencyInfo(NeighborId, BoundsB, ClassB))
			{
				continue;
			}

			++Checked;
			// Use the EXACT predicate placement uses to create mesh links. Keep every
			// edge current code would form (incl. thin/beam/edge, wall/foundation Z
			// overlap); cut only pairs it would not — i.e. the stale cross-gap
			// "wireless" links baked in by older builds. Matching link creation by
			// construction means valid contiguous meshes can never be fragmented.
			if (!FStructuralLightweightIndex::AreBoundsStructurallyConnected(
				BoundsA, BoundsB, ClassA, ClassB))
			{
				ToPrune.Emplace(Pair.Key, NeighborId);
			}
		}
	}

	int32 Pruned = 0;
	for (const TPair<FStructuralNodeId, FStructuralNodeId>& Edge : ToPrune)
	{
		if (FStructuralNodeRecord* RecordA = RuntimeGraph.Find(Edge.Key))
		{
			RecordA->NeighborIds.Remove(Edge.Value);
		}
		if (FStructuralNodeRecord* RecordB = RuntimeGraph.Find(Edge.Value))
		{
			RecordB->NeighborIds.Remove(Edge.Key);
		}

		UFGStructuralPowerConnectionComponent* ConnA = ResolveNodeConnector(Edge.Key, false);
		UFGStructuralPowerConnectionComponent* ConnB = ResolveNodeConnector(Edge.Value, false);
		if (IsValid(ConnA) && IsValid(ConnB) && ConnA->HasHiddenConnection(ConnB))
		{
			ConnA->RemoveHiddenConnection(ConnB);
			++Pruned;
		}
	}

	UE_LOG(LogStructuralPower, Log,
		TEXT("[PWR] load revalidate: checked %d struct edge(s), pruned %d stale cross-gap link(s)"),
		Checked,
		Pruned);
}

int32 AStructuralPowerGraphSubsystem::CountStructuralHiddenNodes() const
{
	int32 Count = 0;
	for (const TPair<FStructuralNodeId, FStructuralNodeRecord>& Pair : RuntimeGraph)
	{
		if (Pair.Value.bIsOutlet)
		{
			continue;
		}

		UFGStructuralPowerConnectionComponent* Hidden = nullptr;
		if (AFGBuildable* Buildable = const_cast<AStructuralPowerGraphSubsystem*>(this)->ResolveBuildable(Pair.Key))
		{
			Hidden = FindStructuralConnector(Buildable);
		}
		else if (Pair.Key.IsLightweight())
		{
			Hidden = LightweightIndex.FindHiddenConnector(FStructuralLightweightIndex::KeyFromNodeId(Pair.Key));
		}

		if (IsValid(Hidden) && Hidden->IsHidden())
		{
			++Count;
		}
	}

	return Count;
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
	FStructuralPowerDiagnostics::AuditWorld(GetWorld(), true);
}
