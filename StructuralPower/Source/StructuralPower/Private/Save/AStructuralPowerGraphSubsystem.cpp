// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Save/AStructuralPowerGraphSubsystem.h"

#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableCircuitBridge.h"
#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "Circuit/FStructuralHiddenConnectionUtil.h"
#include "Components/UFGStructuralPowerConnectionComponent.h"
#include "Config/FStructuralPowerModConfig.h"
#include "Diagnostics/FStructuralPowerDiagnostics.h"
#include "Diagnostics/FStructuralPowerTrace.h"
#include "Engine/World.h"
#include "FGBuildableSubsystem.h"
#include "FGCircuitConnectionComponent.h"
#include "FGCircuitSubsystem.h"
#include "FGLightweightBuildableSubsystem.h"
#include "FGPowerConnectionComponent.h"
#include "Graph/FStructuralAdjacencyHeuristics.h"
#include "Graph/FStructuralAttachmentResolver.h"
#include "Graph/FStructuralOutletParentResolver.h"
#include "Routing/FStructuralPowerRouter.h"
#include "Kismet/GameplayStatics.h"
#include "Lightweight/FStructuralLightweightIndex.h"
#include "Graph/FStructuralSwitchParentResolver.h"
#include "Network/UStructuralPowerSwitchListener.h"
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

/** Vanilla parity: circuit present and connector reports live power (fuse / no production = false). */
static bool ConnectorSuppliesPower(const UFGPowerConnectionComponent* Component)
{
	return ComponentCarriesPower(Component) && Component->HasPower();
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

UFGStructuralPowerConnectionComponent* AStructuralPowerGraphSubsystem::FindBusConnector(const AFGBuildable* Host)
{
	if (!IsValid(Host))
	{
		return nullptr;
	}

	TInlineComponentArray<UFGStructuralPowerConnectionComponent*> Connectors;
	const_cast<AFGBuildable*>(Host)->GetComponents(Connectors);
	for (UFGStructuralPowerConnectionComponent* Connector : Connectors)
	{
		if (IsValid(Connector) && Connector->GetFName() == StructuralPowerConstants::OutletBusConnectorName)
		{
			return Connector;
		}
	}

	return nullptr;
}

void AStructuralPowerGraphSubsystem::StripPersistedEndpointModComponents(AFGBuildable* Host)
{
	if (!IsValid(Host) || !Host->HasAuthority())
	{
		return;
	}

	TInlineComponentArray<UFGStructuralPowerConnectionComponent*> Buses;
	Host->GetComponents(Buses);
	for (UFGStructuralPowerConnectionComponent* Bus : Buses)
	{
		if (!IsValid(Bus) || Bus->GetFName() != StructuralPowerConstants::OutletBusConnectorName)
		{
			continue;
		}

		TArray<UFGCircuitConnectionComponent*> HiddenLinks;
		Bus->GetHiddenConnections(HiddenLinks);
		for (UFGCircuitConnectionComponent* Other : HiddenLinks)
		{
			if (IsValid(Other))
			{
				Other->RemoveHiddenConnection(Bus);
			}
			Bus->RemoveHiddenConnection(Other);
		}
		Bus->ClearHiddenConnections();

		if (UWorld* World = Host->GetWorld())
		{
			if (AFGCircuitSubsystem* CircuitSubsystem = AFGCircuitSubsystem::Get(World))
			{
				CircuitSubsystem->RemoveComponent(Bus);
			}
		}

		Host->RemoveInstanceComponent(Bus);
		Bus->DestroyComponent();
	}

	if (Host->IsA<AFGBuildableCircuitSwitch>())
	{
		TInlineComponentArray<UStructuralPowerSwitchListener*> Listeners;
		Host->GetComponents(Listeners);
		for (UStructuralPowerSwitchListener* Listener : Listeners)
		{
			if (!IsValid(Listener))
			{
				continue;
			}

			Host->RemoveInstanceComponent(Listener);
			Listener->DestroyComponent();
		}
	}
}

UFGStructuralPowerConnectionComponent* AStructuralPowerGraphSubsystem::FindOutletBusConnector(const AFGBuildablePowerPole* Outlet)
{
	return FindBusConnector(Outlet);
}

UFGStructuralPowerConnectionComponent* AStructuralPowerGraphSubsystem::GetOrCreateBusConnector(AFGBuildable* Host)
{
	if (!IsValid(Host))
	{
		return nullptr;
	}

	const bool bPole = Host->IsA<AFGBuildablePowerPole>();
	const bool bSwitch = Host->IsA<AFGBuildableCircuitSwitch>();
	if (!bPole && !bSwitch)
	{
		return nullptr;
	}

	if (UFGStructuralPowerConnectionComponent* Existing = FindBusConnector(Host))
	{
		return Existing;
	}

	UFGStructuralPowerConnectionComponent* Connector = NewObject<UFGStructuralPowerConnectionComponent>(
		Host,
		StructuralPowerConstants::OutletBusConnectorName,
		RF_Transient);
	if (!Connector)
	{
		return nullptr;
	}

	Connector->SetMobility(EComponentMobility::Static);
	Host->AddInstanceComponent(Connector);
	Connector->RegisterComponent();
	return Connector;
}

UFGStructuralPowerConnectionComponent* AStructuralPowerGraphSubsystem::GetOrCreateOutletBusConnector(AFGBuildablePowerPole* Outlet)
{
	return GetOrCreateBusConnector(Outlet);
}

FStructuralWallAnchor AStructuralPowerGraphSubsystem::ResolveOutletAnchor(AFGBuildable* Outlet) const
{
	return FStructuralAttachmentResolver::ResolveStructuralParent(Outlet, GetWorld(), LightweightIndex);
}

FStructuralComponentResolveResult AStructuralPowerGraphSubsystem::ResolveStructuralComponentAt(
	const FVector& WorldLoc,
	float QueryRadiusCm,
	TSubclassOf<AFGBuildable> ClassHint) const
{
	return FStructuralAttachmentResolver::ResolveStructuralComponent(
		StructureGraph,
		WorldLoc,
		QueryRadiusCm,
		ClassHint);
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

	// Seed bridge endpoints through the deferred outlet path so buses mesh and promote on load.
	int32 SeededPoles = 0;
	int32 SeededSwitches = 0;
	if (FStructuralPowerSessionSettings::IsPropagationEnabled())
	{
		if (AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(World))
		{
			for (AFGBuildable* Buildable : BuildableSubsystem->GetAllBuildablesRef())
			{
				if (!IsValid(Buildable) || !Buildable->HasAuthority())
				{
					continue;
				}

				if (FStructuralEligibilityRules::IsPowerBridgePole(Buildable))
				{
					EnqueuePlacement(Buildable, EStructuralPlacementJobType::Outlet, /*bDefer=*/true);
					++SeededPoles;
				}
				else if (FStructuralPowerModConfig::IsGatePowerSwitchesEnabled()
					&& FStructuralEligibilityRules::IsPowerBridgeSwitch(Buildable))
				{
					EnqueuePlacement(Buildable, EStructuralPlacementJobType::Outlet, /*bDefer=*/true);
					++SeededSwitches;
				}
			}
		}
	}

	int32 Components = 0;
	int32 Largest = 0;
	StructureGraph.GetComponentStats(Components, Largest);

	UE_LOG(LogStructuralPower, Log,
		TEXT("Graph ready — %d structure node(s), %d component(s) (largest %d), %d LW tracked,"
			" %d pole(s) seeded, %d switch(es) seeded"),
		StructureGraph.GetNodeCount(),
		Components,
		Largest,
		LightweightIndex.GetTrackedCount(),
		SeededPoles,
		SeededSwitches);
}

void AStructuralPowerGraphSubsystem::PurgeSavedOutletBusMesh(UWorld* World)
{
	AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(World);
	if (!IsValid(BuildableSubsystem))
	{
		return;
	}

	int32 Purged = 0;

	for (AFGBuildable* Buildable : BuildableSubsystem->GetAllBuildablesRef())
	{
		if (!IsValid(Buildable) || !Buildable->HasAuthority())
		{
			continue;
		}

		const bool bPole = FStructuralEligibilityRules::IsPowerBridgePole(Buildable);
		const bool bSwitch = FStructuralPowerModConfig::IsGatePowerSwitchesEnabled()
			&& FStructuralEligibilityRules::IsPowerBridgeSwitch(Buildable);
		if (!bPole && !bSwitch)
		{
			continue;
		}

		UFGStructuralPowerConnectionComponent* Bus = FindBusConnector(Buildable);
		if (!IsValid(Bus))
		{
			continue;
		}

		StripPersistedEndpointModComponents(Buildable);
		++Purged;
	}

	if (Purged > 0)
	{
		UE_LOG(LogStructuralPower, Log,
			TEXT("Purged saved outlet-bus mesh on %d endpoint(s) — rebuilding from geometry"),
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

FStructuralComponentKey AStructuralPowerGraphSubsystem::MakeComponentKeyForRoot(int32 ComponentRoot) const
{
	FStructuralComponentKey Key;
	Key.CanonicalNodeId = StructureGraph.MakeCanonicalNodeIdForComponent(ComponentRoot);
	return Key;
}

FStructuralComponentKey AStructuralPowerGraphSubsystem::MakeComponentKeyForParent(
	const FStructuralNodeId& ParentId) const
{
	return MakeComponentKeyForRoot(StructureGraph.FindRoot(ParentId));
}

FName AStructuralPowerGraphSubsystem::GetPlayerOverrideId(const AFGBuildable* Buildable) const
{
	if (!IsValid(Buildable))
	{
		return NAME_None;
	}

	const FStructuralNodeId NodeId = MakeNodeId(Buildable);
	if (const FName* Override = PlayerOverrideIds.Find(NodeId))
	{
		return *Override;
	}

	return NAME_None;
}

FName AStructuralPowerGraphSubsystem::GetOrCreateComponentDefaultId(
	const FStructuralComponentKey& ComponentKey)
{
	if (!ComponentKey.IsValid())
	{
		return NAME_None;
	}

	if (const FName* Existing = ComponentDefaultIds.Find(ComponentKey))
	{
		return *Existing;
	}

	const FName Created = FStructuralPowerRouter::MakeDefaultIdName(ComponentKey.CanonicalNodeId);
	ComponentDefaultIds.Add(ComponentKey, Created);
	return Created;
}

FName AStructuralPowerGraphSubsystem::ResolveEffectiveId(
	AFGBuildable* Buildable,
	EStructuralChannel Tag)
{
	if (!IsValid(Buildable))
	{
		return NAME_None;
	}

	if (Tag == EStructuralChannel::Structure)
	{
		return NAME_None;
	}

	if (const FName Override = GetPlayerOverrideId(Buildable); !Override.IsNone())
	{
		return Override;
	}

	if (Tag == EStructuralChannel::Switch)
	{
		if (const AFGBuildableCircuitSwitch* Switch = Cast<AFGBuildableCircuitSwitch>(Buildable))
		{
			if (const FName SwitchId = FStructuralPowerRouter::ResolveSwitchEffectiveId(Switch);
				!SwitchId.IsNone())
			{
				return SwitchId;
			}
		}
	}

	FStructuralComponentKey ComponentKey;
	if (const FTrackedEndpoint* Endpoint = TrackedEndpoints.Find(MakeNodeId(Buildable)))
	{
		ComponentKey = MakeComponentKeyForParent(Endpoint->ParentId);
	}
	else
	{
		const FStructuralWallAnchor Anchor = FStructuralAttachmentResolver::ResolveStructuralParent(
			Buildable,
			GetWorld(),
			LightweightIndex);
		if (Anchor.IsValid())
		{
			ComponentKey = MakeComponentKeyForParent(MakeParentNodeId(Anchor));
		}
	}

	if (!ComponentKey.IsValid())
	{
		return NAME_None;
	}

	return GetOrCreateComponentDefaultId(ComponentKey);
}

FStructuralChannelKey AStructuralPowerGraphSubsystem::ResolveChannelKeyForBuildable(
	AFGBuildable* Buildable)
{
	FStructuralChannelKey Key;
	if (!IsValid(Buildable))
	{
		return Key;
	}

	Key.Tag = FStructuralEligibilityRules::ClassifyBuildable(Buildable);
	Key.EffectiveId = ResolveEffectiveId(Buildable, Key.Tag);
	return Key;
}

void AStructuralPowerGraphSubsystem::SetPlayerOverrideId(
	AFGBuildable* Buildable,
	FName PlayerOverrideId)
{
	if (!IsValid(Buildable) || !Buildable->HasAuthority())
	{
		return;
	}

	const FStructuralNodeId NodeId = MakeNodeId(Buildable);
	if (PlayerOverrideId.IsNone())
	{
		PlayerOverrideIds.Remove(NodeId);
	}
	else
	{
		PlayerOverrideIds.Add(NodeId, PlayerOverrideId);
	}
}

int32 AStructuralPowerGraphSubsystem::ResolveEndpointComponentRoot(
	AFGBuildable* Endpoint,
	const FStructuralWallAnchor& Anchor,
	FStructuralNodeId& OutParentId)
{
	if (EnsureParentRegisteredInGraph(Anchor, OutParentId))
	{
		return StructureGraph.FindRoot(OutParentId);
	}

	return FStructuralAttachmentResolver::ResolveComponentRootForBuildable(
		Endpoint,
		StructureGraph,
		LightweightIndex,
		GetWorld(),
		OutParentId);
}

bool AStructuralPowerGraphSubsystem::EnsureParentRegisteredInGraph(
	const FStructuralWallAnchor& Anchor,
	FStructuralNodeId& OutParentId)
{
	if (!Anchor.IsValid())
	{
		return false;
	}

	const FStructuralNodeId ParentId = MakeParentNodeId(Anchor);
	if (StructureGraph.FindRoot(ParentId) != INDEX_NONE)
	{
		OutParentId = ParentId;
		return true;
	}

	if (IsValid(Anchor.Actor) && FStructuralEligibilityRules::IsBusMember(Anchor.Actor))
	{
		ProcessStructure(Anchor.Actor);
	}
	else if (Anchor.Lightweight.IsValid())
	{
		ProcessLightweightStructure(Anchor.Lightweight);
	}
	else
	{
		return false;
	}

	if (StructureGraph.FindRoot(ParentId) != INDEX_NONE)
	{
		OutParentId = ParentId;
		UE_LOG(LogStructuralPower, Verbose,
			TEXT("[PWR] parent ingested into structure graph actor=%s lw=%s[%d]"),
			IsValid(Anchor.Actor) ? *Anchor.Actor->GetName() : TEXT("null"),
			Anchor.Lightweight.IsValid() ? *Anchor.Lightweight.BuildableClass->GetName() : TEXT("null"),
			Anchor.Lightweight.IsValid() ? Anchor.Lightweight.Index : INDEX_NONE);
		return true;
	}

	return false;
}

void AStructuralPowerGraphSubsystem::LinkBusToVisibleConnections(
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

			LinkHiddenPair(Bus, Visible);
		}
		return;
	}

	if (AFGBuildableCircuitBridge* Bridge = Cast<AFGBuildableCircuitBridge>(Host))
	{
		if (UFGCircuitConnectionComponent* Conn0 = Bridge->GetConnection0())
		{
			if (UFGPowerConnectionComponent* Visible = Cast<UFGPowerConnectionComponent>(Conn0))
			{
				LinkHiddenPair(Bus, Visible);
			}
		}
		if (UFGCircuitConnectionComponent* Conn1 = Bridge->GetConnection1())
		{
			if (UFGPowerConnectionComponent* Visible = Cast<UFGPowerConnectionComponent>(Conn1))
			{
				LinkHiddenPair(Bus, Visible);
			}
		}
	}
}

UFGCircuitConnectionComponent* AStructuralPowerGraphSubsystem::GetComponentSourceConnector(
	int32 ComponentRoot,
	const AFGBuildable* ExcludeHost)
{
	if (ComponentRoot == INDEX_NONE)
	{
		return nullptr;
	}

	UFGPowerConnectionComponent* PoweredVisible = nullptr;
	UFGStructuralPowerConnectionComponent* PoweredBus = nullptr;

	for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : TrackedEndpoints)
	{
		if (StructureGraph.FindRoot(Pair.Value.ParentId) != ComponentRoot)
		{
			continue;
		}

		AFGBuildable* Host = Pair.Value.Actor.Get();
		if (!IsValid(Host) || Host == ExcludeHost)
		{
			continue;
		}

		if (UFGStructuralPowerConnectionComponent* Bus = FindBusConnector(Host))
		{
			if (!PoweredBus && ComponentCarriesPower(Bus))
			{
				PoweredBus = Bus;
			}
		}

		if (AFGBuildablePowerPole* Pole = Cast<AFGBuildablePowerPole>(Host))
		{
			for (UFGPowerConnectionComponent* Visible : Pole->GetPowerConnections())
			{
				if (IsValid(Visible) && !Visible->IsHidden() && ComponentCarriesPower(Visible))
				{
					PoweredVisible = Visible;
					break;
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
						if (IsValid(Visible) && ComponentCarriesPower(Visible))
						{
							PoweredVisible = Visible;
						}
					}
				}
				if (!PoweredVisible)
				{
					if (UFGCircuitConnectionComponent* Conn1 = Switch->GetConnection1())
					{
						if (UFGPowerConnectionComponent* Visible = Cast<UFGPowerConnectionComponent>(Conn1))
						{
							if (IsValid(Visible) && ComponentCarriesPower(Visible))
							{
								PoweredVisible = Visible;
							}
						}
					}
				}
			}
		}
	}

	if (PoweredVisible)
	{
		return PoweredVisible;
	}

	if (PoweredBus)
	{
		return PoweredBus;
	}

	for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : TrackedEndpoints)
	{
		if (StructureGraph.FindRoot(Pair.Value.ParentId) != ComponentRoot)
		{
			continue;
		}

		AFGBuildable* Host = Pair.Value.Actor.Get();
		if (!IsValid(Host) || Host == ExcludeHost)
		{
			continue;
		}

		if (UFGStructuralPowerConnectionComponent* Bus = FindBusConnector(Host))
		{
			if (UFGStructuralPowerConnectionComponent* Reachable = FindPoweredHiddenReachable(Bus))
			{
				return Reachable;
			}
		}
	}

	return nullptr;
}

bool AStructuralPowerGraphSubsystem::DoesComponentRootCarryPower(int32 ComponentRoot) const
{
	if (ComponentRoot == INDEX_NONE
		|| !FStructuralPowerSessionSettings::IsPropagationEnabled())
	{
		return false;
	}

	if (UFGCircuitConnectionComponent* Source = const_cast<AStructuralPowerGraphSubsystem*>(this)
			->GetComponentSourceConnector(ComponentRoot, nullptr))
	{
		return ComponentCarriesPower(Cast<UFGPowerConnectionComponent>(Source));
	}

	return false;
}

bool AStructuralPowerGraphSubsystem::QueryHoverpackStructuralAnchor(
	const FVector& QueryLoc,
	float MaxHorizontal,
	float MaxVertical,
	FStructuralHoverpackAnchorQuery& Out) const
{
	Out = FStructuralHoverpackAnchorQuery{};

	if (MaxHorizontal <= 0.0f
		|| MaxVertical <= 0.0f
		|| !FStructuralPowerSessionSettings::IsPropagationEnabled()
		|| !FStructuralPowerModConfig::IsHoverpackStructuralEnabled())
	{
		return false;
	}

	int32 ComponentRoot = INDEX_NONE;
	if (!StructureGraph.FindNearestStructureAnchor(
			QueryLoc,
			MaxHorizontal,
			MaxVertical,
			Out.Anchor,
			ComponentRoot))
	{
		return false;
	}

	Out.ComponentRoot = ComponentRoot;

	if (UFGCircuitConnectionComponent* Source = const_cast<AStructuralPowerGraphSubsystem*>(this)
			->GetComponentSourceConnector(ComponentRoot, nullptr))
	{
		if (UFGPowerConnectionComponent* Feed = Cast<UFGPowerConnectionComponent>(Source))
		{
			Out.FeedConnector = Feed;
			Out.bPowered = ConnectorSuppliesPower(Feed);
		}
	}

	Out.bFound = true;
	return true;
}

void AStructuralPowerGraphSubsystem::TearDownSwitchStructuralLinks(AFGBuildable* Host)
{
	UFGStructuralPowerConnectionComponent* Bus = FindBusConnector(Host);
	if (!IsValid(Bus))
	{
		return;
	}

	TArray<UFGCircuitConnectionComponent*> HiddenLinks;
	Bus->GetHiddenConnections(HiddenLinks);
	for (UFGCircuitConnectionComponent* OtherRaw : HiddenLinks)
	{
		Bus->RemoveHiddenConnection(OtherRaw);
		if (IsValid(OtherRaw))
		{
			OtherRaw->RemoveHiddenConnection(Bus);
		}
	}
}

void AStructuralPowerGraphSubsystem::StripInactiveSwitchStructuralLinks(int32 Root)
{
	if (Root == INDEX_NONE || !FStructuralPowerModConfig::IsGatePowerSwitchesEnabled())
	{
		return;
	}

	for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : TrackedEndpoints)
	{
		if (Pair.Value.Kind != EStructuralEndpointKind::Switch
			|| StructureGraph.FindRoot(Pair.Value.ParentId) != Root)
		{
			continue;
		}

		AFGBuildableCircuitSwitch* Switch = Pair.Value.GetSwitch();
		if (!IsValid(Switch) || Switch->IsBridgeActive())
		{
			continue;
		}

		TearDownSwitchStructuralLinks(Switch);
	}
}

bool AStructuralPowerGraphSubsystem::ShouldEndpointParticipateInRestitch(
	AFGBuildable* Host,
	EStructuralEndpointKind Kind)
{
	if (!IsValid(Host))
	{
		return false;
	}

	if (Kind == EStructuralEndpointKind::Pole)
	{
		return true;
	}

	if (Kind != EStructuralEndpointKind::Switch
		|| !FStructuralPowerModConfig::IsGatePowerSwitchesEnabled())
	{
		return false;
	}

	const AFGBuildableCircuitSwitch* Switch = Cast<AFGBuildableCircuitSwitch>(Host);
	return IsValid(Switch) && Switch->IsBridgeActive();
}

bool AStructuralPowerGraphSubsystem::ShouldMeshEndpoints(
	AFGBuildable* HostA,
	AFGBuildable* HostB,
	int32 ComponentRoot) const
{
	if (!FStructuralPowerModConfig::IsPowerSwitchManualGroupsEnabled())
	{
		return true;
	}

	const FStructuralChannelKey KeyA = const_cast<AStructuralPowerGraphSubsystem*>(this)
		->ResolveChannelKeyForBuildable(HostA);
	const FStructuralChannelKey KeyB = const_cast<AStructuralPowerGraphSubsystem*>(this)
		->ResolveChannelKeyForBuildable(HostB);
	const FStructuralComponentKey CompKey = MakeComponentKeyForRoot(ComponentRoot);

	if (KeyA.Tag != EStructuralChannel::Switch && KeyB.Tag != EStructuralChannel::Switch)
	{
		return true;
	}

	const FName SwitchId = KeyA.Tag == EStructuralChannel::Switch ? KeyA.EffectiveId : KeyB.EffectiveId;
	const FName DeviceId = KeyA.Tag == EStructuralChannel::Switch ? KeyB.EffectiveId : KeyA.EffectiveId;
	const EStructuralChannel PeerTag = KeyA.Tag == EStructuralChannel::Switch ? KeyB.Tag : KeyA.Tag;

	// Import/export (DR-001): wired switch ON merges grid with default structure physical bus.
	if (PeerTag == EStructuralChannel::Structure)
	{
		return true;
	}

	return FStructuralPowerRouter::ShouldRouteSwitchGate(SwitchId, DeviceId, CompKey, CompKey);
}

void AStructuralPowerGraphSubsystem::EnsureSwitchListener(AFGBuildableCircuitSwitch* Switch)
{
	if (!IsValid(Switch))
	{
		return;
	}

	TInlineComponentArray<UStructuralPowerSwitchListener*> Listeners;
	Switch->GetComponents(Listeners);
	if (Listeners.Num() > 0)
	{
		return;
	}

	UStructuralPowerSwitchListener* Listener = NewObject<UStructuralPowerSwitchListener>(Switch, NAME_None, RF_Transient);
	if (!Listener)
	{
		return;
	}

	Switch->AddInstanceComponent(Listener);
	Listener->RegisterComponent();
	Listener->BindSubsystem(this, Switch);
}

void AStructuralPowerGraphSubsystem::RestitchSwitchKeyedSubnet(
	AFGBuildableCircuitSwitch* Switch,
	UFGStructuralPowerConnectionComponent* OutletBus,
	int32 ComponentRoot,
	const FStructuralNodeId& SwitchNodeId)
{
	if (!IsValid(Switch) || !IsValid(OutletBus) || ComponentRoot == INDEX_NONE)
	{
		return;
	}

	if (UFGCircuitConnectionComponent* Feed = GetComponentSourceConnector(ComponentRoot, Switch))
	{
		if (UFGPowerConnectionComponent* FeedPower = Cast<UFGPowerConnectionComponent>(Feed))
		{
			LinkHiddenPair(OutletBus, FeedPower);
		}
	}

	for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : TrackedEndpoints)
	{
		if (Pair.Key == SwitchNodeId)
		{
			continue;
		}

		AFGBuildable* SiblingHost = Pair.Value.Actor.Get();
		if (!IsValid(SiblingHost)
			|| StructureGraph.FindRoot(Pair.Value.ParentId) != ComponentRoot)
		{
			continue;
		}

		if (!ShouldMeshEndpoints(Switch, SiblingHost, ComponentRoot))
		{
			continue;
		}

		if (UFGStructuralPowerConnectionComponent* SiblingBus = FindBusConnector(SiblingHost))
		{
			LinkHiddenPair(OutletBus, SiblingBus);
		}
	}

	UFGStructuralPowerConnectionComponent* Seed = ComponentCarriesPower(OutletBus)
		? OutletBus
		: FindPoweredHiddenReachable(OutletBus);
	if (Seed)
	{
		PromoteStructuralMeshFrom(Seed);
	}
}

void AStructuralPowerGraphSubsystem::OnSwitchStateChanged(AFGBuildableCircuitSwitch* Switch)
{
	if (!IsValid(Switch) || !Switch->HasAuthority())
	{
		return;
	}

	if (!FStructuralPowerSessionSettings::IsPropagationEnabled()
		|| !FStructuralPowerModConfig::IsGatePowerSwitchesEnabled())
	{
		return;
	}

	const FStructuralNodeId SwitchId = MakeNodeId(Switch);
	if (FTrackedEndpoint* Tracked = TrackedEndpoints.Find(SwitchId))
	{
		const FStructuralSwitchParentResolveResult ParentResolve =
			FStructuralSwitchParentResolver::Resolve(
				Switch,
				GetWorld(),
				StructureGraph,
				LightweightIndex);
		FStructuralNodeId ParentId;
		const int32 Root = ResolveEndpointComponentRoot(
			Switch,
			ParentResolve.Anchor,
			ParentId);
		if (ParentResolve.IsValid())
		{
			Tracked->ParentId = ParentId;
		}

		if (!Switch->IsBridgeActive())
		{
			TearDownSwitchStructuralLinks(Switch);
			if (!FStructuralPowerModConfig::IsPowerSwitchManualGroupsEnabled()
				&& Root != INDEX_NONE)
			{
				TArray<int32> Roots;
				Roots.Add(Root);
				ReEnergizeComponentRoots(Roots, /*bTearDownFirst=*/true);
			}
		}
		else
		{
			UFGStructuralPowerConnectionComponent* OutletBus = GetOrCreateBusConnector(Switch);
			if (OutletBus)
			{
				LinkBusToVisibleConnections(Switch, OutletBus);
				if (Root != INDEX_NONE)
				{
					if (FStructuralPowerModConfig::IsPowerSwitchManualGroupsEnabled())
					{
						RestitchSwitchKeyedSubnet(Switch, OutletBus, Root, SwitchId);
					}
					else
					{
						TArray<int32> Roots;
						Roots.Add(Root);
						ReEnergizeComponentRoots(Roots, /*bTearDownFirst=*/true);
					}
				}
				else if (UFGStructuralPowerConnectionComponent* Seed = ComponentCarriesPower(OutletBus)
					? OutletBus
					: FindPoweredHiddenReachable(OutletBus))
				{
					PromoteStructuralMeshFrom(Seed);
				}
			}
		}

		FStructuralPowerTrace::LogHook(
			Switch,
			TEXT("OnIsSwitchOnChanged"),
			Switch->IsBridgeActive() ? TEXT("restitch_on") : TEXT("restitch_off"),
			nullptr);
		return;
	}

	EnqueuePlacement(Switch, EStructuralPlacementJobType::Outlet, /*bDefer=*/true);
}

void AStructuralPowerGraphSubsystem::RestitchComponent(int32 Root, bool bTearDownFirst)
{
	if (Root == INDEX_NONE)
	{
		return;
	}

	StripInactiveSwitchStructuralLinks(Root);

	TArray<AFGBuildable*> Hosts;
	TArray<UFGStructuralPowerConnectionComponent*> Buses;
	for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : TrackedEndpoints)
	{
		AFGBuildable* Host = Pair.Value.Actor.Get();
		if (!IsValid(Host))
		{
			continue;
		}

		if (StructureGraph.FindRoot(Pair.Value.ParentId) != Root)
		{
			continue;
		}

		if (!ShouldEndpointParticipateInRestitch(Host, Pair.Value.Kind))
		{
			continue;
		}

		UFGStructuralPowerConnectionComponent* Bus = GetOrCreateBusConnector(Host);
		if (!Bus)
		{
			continue;
		}

		Hosts.Add(Host);
		Buses.Add(Bus);
	}

	if (Buses.Num() == 0)
	{
		return;
	}

	if (bTearDownFirst)
	{
		// Drop only bus-to-bus hidden links; keep each bus↔visible link so wired poles never flicker.
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
		LinkBusToVisibleConnections(Hosts[Index], Buses[Index]);
		if (Index > 0 && ShouldMeshEndpoints(Hosts[0], Hosts[Index], Root))
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
	if (!IsValid(Buildable))
	{
		return;
	}

	if (FStructuralPowerModConfig::IsGatePowerSwitchesEnabled()
		&& FStructuralEligibilityRules::IsPowerBridgeSwitch(Buildable))
	{
		ProcessSwitchEndpoint(Cast<AFGBuildableCircuitSwitch>(Buildable));
		return;
	}

	if (FStructuralEligibilityRules::IsPowerBridgePole(Buildable))
	{
		ProcessPoleEndpoint(Cast<AFGBuildablePowerPole>(Buildable));
		return;
	}

	FStructuralPowerTrace::LogPlacementSkip(Buildable, TEXT("not_bridge_endpoint"));
}

void AStructuralPowerGraphSubsystem::ProcessPoleEndpoint(AFGBuildablePowerPole* Pole)
{
	if (!IsValid(Pole))
	{
		return;
	}

	UFGStructuralPowerConnectionComponent* OutletBus = GetOrCreateBusConnector(Pole);
	if (!OutletBus)
	{
		FStructuralPowerTrace::LogPlacementSkip(Pole, TEXT("outlet_bus_create_failed"));
		return;
	}

	const FStructuralWallAnchor ParentAnchor = ResolveOutletAnchor(Pole);
	FStructuralNodeId ParentId;
	const int32 Root = ResolveEndpointComponentRoot(Pole, ParentAnchor, ParentId);

	const FStructuralNodeId PoleId = MakeNodeId(Pole);
	FTrackedEndpoint& Tracked = TrackedEndpoints.FindOrAdd(PoleId);
	Tracked.Actor = Pole;
	Tracked.ParentId = ParentId;
	Tracked.Kind = EStructuralEndpointKind::Pole;
	RegisterBuildableActor(Pole);

	LinkBusToVisibleConnections(Pole, OutletBus);

	if (Root != INDEX_NONE)
	{
		for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : TrackedEndpoints)
		{
			if (Pair.Key == PoleId)
			{
				continue;
			}

			AFGBuildable* SiblingHost = Pair.Value.Actor.Get();
			if (!IsValid(SiblingHost) || StructureGraph.FindRoot(Pair.Value.ParentId) != Root)
			{
				continue;
			}

			if (!ShouldEndpointParticipateInRestitch(SiblingHost, Pair.Value.Kind))
			{
				continue;
			}

			if (UFGStructuralPowerConnectionComponent* SiblingBus = FindBusConnector(SiblingHost))
			{
				if (ShouldMeshEndpoints(Pole, SiblingHost, Root))
				{
					LinkHiddenPair(OutletBus, SiblingBus);
					break;
				}
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
		TEXT("[PWR] outlet %s root=%d parentValid=%d busCircuit=%d powered=%d tag=%s id=%s"),
		*Pole->GetName(),
		Root,
		ParentAnchor.IsValid() ? 1 : 0,
		OutletBus->GetCircuitID(),
		ComponentCarriesPower(OutletBus) ? 1 : 0,
		StructuralChannelToString(EStructuralChannel::Structure),
		TEXT("-"));
}

void AStructuralPowerGraphSubsystem::ProcessSwitchEndpoint(AFGBuildableCircuitSwitch* Switch)
{
	if (!IsValid(Switch) || !FStructuralPowerModConfig::IsGatePowerSwitchesEnabled())
	{
		FStructuralPowerTrace::LogPlacementSkip(Switch, TEXT("switch_gating_disabled"));
		return;
	}

	UFGStructuralPowerConnectionComponent* OutletBus = GetOrCreateBusConnector(Switch);
	if (!OutletBus)
	{
		FStructuralPowerTrace::LogPlacementSkip(Switch, TEXT("switch_bus_create_failed"));
		return;
	}

	const FStructuralSwitchParentResolveResult ParentResolve =
		FStructuralSwitchParentResolver::Resolve(
			Switch,
			GetWorld(),
			StructureGraph,
			LightweightIndex);
	const FStructuralWallAnchor ParentAnchor = ParentResolve.Anchor;
	FStructuralNodeId ParentId;
	const int32 Root = ResolveEndpointComponentRoot(Switch, ParentAnchor, ParentId);

	const FStructuralNodeId SwitchId = MakeNodeId(Switch);
	FTrackedEndpoint& Tracked = TrackedEndpoints.FindOrAdd(SwitchId);
	Tracked.Actor = Switch;
	Tracked.ParentId = ParentId;
	Tracked.Kind = EStructuralEndpointKind::Switch;
	RegisterBuildableActor(Switch);
	EnsureSwitchListener(Switch);

	const FStructuralChannelKey ChannelKey = ResolveChannelKeyForBuildable(Switch);

	if (!Switch->IsBridgeActive())
	{
		TearDownSwitchStructuralLinks(Switch);
		UE_LOG(LogStructuralPower, Log,
			TEXT("[PWR] outlet %s root=%d parentValid=%d busCircuit=%d powered=%d tag=%s id=%s"
				" wirePort=%s"),
			*Switch->GetName(),
			Root,
			ParentAnchor.IsValid() ? 1 : 0,
			OutletBus->GetCircuitID(),
			0,
			StructuralChannelToString(ChannelKey.Tag),
			*FStructuralPowerTrace::FormatEffectiveIdForTrace(ChannelKey.Tag, ChannelKey.EffectiveId),
			ParentResolve.WirePortIndex == 0
				? TEXT("A")
				: (ParentResolve.WirePortIndex == 1 ? TEXT("B") : TEXT("-")));
		return;
	}

	LinkBusToVisibleConnections(Switch, OutletBus);

	if (Root != INDEX_NONE)
	{
		if (FStructuralPowerModConfig::IsPowerSwitchManualGroupsEnabled())
		{
			RestitchSwitchKeyedSubnet(Switch, OutletBus, Root, SwitchId);
		}
		else
		{
			TArray<int32> Roots;
			Roots.Add(Root);
			ReEnergizeComponentRoots(Roots, /*bTearDownFirst=*/false);
		}
	}
	else
	{
		UFGStructuralPowerConnectionComponent* Seed = ComponentCarriesPower(OutletBus)
			? OutletBus
			: FindPoweredHiddenReachable(OutletBus);
		if (Seed)
		{
			PromoteStructuralMeshFrom(Seed);
		}
	}

	UE_LOG(LogStructuralPower, Log,
		TEXT("[PWR] outlet %s root=%d parentValid=%d busCircuit=%d powered=%d tag=%s id=%s"
			" wirePort=%s"),
		*Switch->GetName(),
		Root,
		ParentAnchor.IsValid() ? 1 : 0,
		OutletBus->GetCircuitID(),
		ComponentCarriesPower(OutletBus) ? 1 : 0,
		StructuralChannelToString(ChannelKey.Tag),
		*FStructuralPowerTrace::FormatEffectiveIdForTrace(ChannelKey.Tag, ChannelKey.EffectiveId),
		ParentResolve.WirePortIndex == 0
			? TEXT("A")
			: (ParentResolve.WirePortIndex == 1 ? TEXT("B") : TEXT("-")));
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

	if (FTrackedEndpoint* Tracked = TrackedEndpoints.Find(NodeId))
	{
		const int32 OldRoot = StructureGraph.FindRoot(Tracked->ParentId);

		if (AFGBuildable* Host = Tracked->Actor.Get())
		{
			if (Tracked->Kind == EStructuralEndpointKind::Switch)
			{
				TearDownSwitchStructuralLinks(Host);
			}
			else if (UFGStructuralPowerConnectionComponent* Bus = FindBusConnector(Host))
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

		TrackedEndpoints.Remove(NodeId);

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
			|| FStructuralEligibilityRules::IsPowerBridgePole(Buildable)
			|| (FStructuralPowerModConfig::IsGatePowerSwitchesEnabled()
				&& FStructuralEligibilityRules::IsPowerBridgeSwitch(Buildable)))
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
