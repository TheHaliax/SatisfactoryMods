// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Save/AStructuralPowerGraphSubsystem.h"

#include "Attach/FStructuralDeviceAttach.h"
#include "Attach/FStructuralPanelAttach.h"
#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableLightSource.h"
#include "Buildables/FGBuildableLightsControlPanel.h"
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
#include "Network/UStructuralPowerPanelListener.h"
#include "Panel/FStructuralPanelControlledSync.h"
#include "Panel/FStructuralPanelPortResolver.h"
#include "Rules/FStructuralEligibilityRules.h"
#include "Session/FStructuralPowerSessionSettings.h"
#include "StructuralPowerConstants.h"
#include "StructuralPowerLog.h"
#include "HAL/PlatformTime.h"

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

	const int32 CircuitA = A->GetCircuitID();
	const int32 CircuitB = B->GetCircuitID();
	if (CircuitA != INDEX_NONE && CircuitA == CircuitB)
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

struct FCircuitPromotionScope
{
	AStructuralPowerGraphSubsystem* Graph = nullptr;

	explicit FCircuitPromotionScope(AStructuralPowerGraphSubsystem* InGraph)
		: Graph(InGraph)
	{
		if (Graph)
		{
			Graph->BeginCircuitPromotion();
		}
	}

	~FCircuitPromotionScope()
	{
		if (Graph)
		{
			Graph->EndCircuitPromotion();
		}
	}
};
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

UFGStructuralPowerConnectionComponent* AStructuralPowerGraphSubsystem::FindPanelControlBus(
	const AFGBuildable* Host)
{
	if (!IsValid(Host))
	{
		return nullptr;
	}

	TInlineComponentArray<UFGStructuralPowerConnectionComponent*> Connectors;
	const_cast<AFGBuildable*>(Host)->GetComponents(Connectors);
	for (UFGStructuralPowerConnectionComponent* Connector : Connectors)
	{
		if (IsValid(Connector)
			&& Connector->GetFName() == StructuralPowerConstants::PanelControlBusConnectorName)
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
		if (!IsValid(Bus))
		{
			continue;
		}

		const FName BusName = Bus->GetFName();
		if (BusName != StructuralPowerConstants::OutletBusConnectorName
			&& BusName != StructuralPowerConstants::PanelControlBusConnectorName)
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

	if (Host->IsA<AFGBuildableLightsControlPanel>())
	{
		TInlineComponentArray<UStructuralPowerPanelListener*> Listeners;
		Host->GetComponents(Listeners);
		for (UStructuralPowerPanelListener* Listener : Listeners)
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

UFGStructuralPowerConnectionComponent* AStructuralPowerGraphSubsystem::GetOrCreatePanelControlBus(
	AFGBuildableLightsControlPanel* Panel)
{
	if (!IsValid(Panel))
	{
		return nullptr;
	}

	if (UFGStructuralPowerConnectionComponent* Existing = FindPanelControlBus(Panel))
	{
		return Existing;
	}

	UFGStructuralPowerConnectionComponent* Connector = NewObject<UFGStructuralPowerConnectionComponent>(
		Panel,
		StructuralPowerConstants::PanelControlBusConnectorName,
		RF_Transient);
	if (!Connector)
	{
		return nullptr;
	}

	Connector->SetMobility(EComponentMobility::Static);
	Panel->AddInstanceComponent(Connector);
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

	bBulkLoadDrainActive = (SeededPoles + SeededSwitches) > 0;
	if (bBulkLoadDrainActive)
	{
		BridgeEndpointsByRoot.Empty();
		bBridgeEndpointRootIndexDirty = false;
	}
	else
	{
		MarkBridgeEndpointRootIndexDirty();
	}

	if (FStructuralPowerModConfig::IsGroupLightingEnabled())
	{
		bPendingPostLoadLightReconcile = true;
	}
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
		const bool bPanel = FStructuralPowerModConfig::IsGroupLightingEnabled()
			&& Buildable->IsA<AFGBuildableLightsControlPanel>();
		if (!bPole && !bSwitch && !bPanel)
		{
			continue;
		}

		const bool bHasModBus = IsValid(FindBusConnector(Buildable))
			|| (bPanel && IsValid(FindPanelControlBus(Buildable)));
		if (!bHasModBus)
		{
			continue;
		}

		StripPersistedEndpointModComponents(Buildable);
		++Purged;
	}

	if (Purged > 0)
	{
		UE_LOG(LogStructuralPower, Log,
			TEXT("Purged saved mod bus mesh on %d endpoint(s) — rebuilding from geometry"),
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

bool AStructuralPowerGraphSubsystem::IsBuildablePlacementPending(AFGBuildable* Buildable) const
{
	if (!IsValid(Buildable))
	{
		return false;
	}

	return IsBuildableAlreadyPending(Buildable, EStructuralPlacementJobType::Outlet)
		|| IsBuildableAlreadyPending(Buildable, EStructuralPlacementJobType::Structure);
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
	const double BulkTickStartSec = bBulkLoadDrainActive ? FPlatformTime::Seconds() : 0.0;

	while (Processed < MaxJobs)
	{
		if (bBulkLoadDrainActive
			&& (FPlatformTime::Seconds() - BulkTickStartSec)
				>= StructuralPowerConstants::BulkLoadDrainTickBudgetSec)
		{
			break;
		}

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

	MaybeRunPostLoadLightReconcile();
}

void AStructuralPowerGraphSubsystem::BeginCircuitPromotion()
{
	++CircuitPromotionDepth;
}

void AStructuralPowerGraphSubsystem::EndCircuitPromotion()
{
	CircuitPromotionDepth = FMath::Max(0, CircuitPromotionDepth - 1);
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
		FCircuitPromotionScope PromotionScope(this);
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
		FCircuitPromotionScope PromotionScope(this);
		PromoteCircuitLink(A, B);
	}

	return bAdded;
}

bool AStructuralPowerGraphSubsystem::LinkHiddenPairLocal(
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
		FCircuitPromotionScope PromotionScope(this);
		PromoteCircuitLink(A, B, ELogVerbosity::Verbose);
	}

	return bAdded;
}

void AStructuralPowerGraphSubsystem::MarkBridgeEndpointRootIndexDirty()
{
	bBridgeEndpointRootIndexDirty = true;
	SourceConnectorByRoot.Empty();
}

void AStructuralPowerGraphSubsystem::RefreshBridgeEndpointRootIndex()
{
	if (!bBridgeEndpointRootIndexDirty)
	{
		return;
	}

	BridgeEndpointsByRoot.Empty();

	for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : TrackedEndpoints)
	{
		if (Pair.Value.Kind != EStructuralEndpointKind::Pole
			&& Pair.Value.Kind != EStructuralEndpointKind::Switch)
		{
			continue;
		}

		if (!Pair.Value.ParentId.IsValid())
		{
			continue;
		}

		const int32 Root = StructureGraph.FindRoot(Pair.Value.ParentId);
		if (Root != INDEX_NONE)
		{
			BridgeEndpointsByRoot.FindOrAdd(Root).Add(Pair.Key);
		}
	}

	bBridgeEndpointRootIndexDirty = false;
}

void AStructuralPowerGraphSubsystem::AddBridgeEndpointToRootIndex(
	const FStructuralNodeId& EndpointId,
	int32 Root)
{
	if (Root == INDEX_NONE || !EndpointId.IsValid())
	{
		return;
	}

	BridgeEndpointsByRoot.FindOrAdd(Root).AddUnique(EndpointId);
}

int32 AStructuralPowerGraphSubsystem::FindRootForTrackedEndpoint(
	const FTrackedEndpoint& Tracked) const
{
	if (!Tracked.ParentId.IsValid())
	{
		return INDEX_NONE;
	}

	return StructureGraph.FindRoot(Tracked.ParentId);
}

int32 AStructuralPowerGraphSubsystem::ResolveBridgeRootFromAnchor(
	AFGBuildable* Host,
	const FStructuralWallAnchor& Anchor,
	FStructuralNodeId& OutParentId,
	bool bPreferBulkResolve)
{
	if (bPreferBulkResolve)
	{
		if (AFGBuildablePowerPole* Pole = Cast<AFGBuildablePowerPole>(Host))
		{
			return ResolvePoleComponentRoot(Pole, Anchor, OutParentId);
		}

		return ResolveBridgeComponentRootBulk(Host, Anchor, OutParentId);
	}

	return ResolveEndpointComponentRoot(Host, Anchor, OutParentId);
}

void AStructuralPowerGraphSubsystem::PromoteStructuralMeshFrom(UFGPowerConnectionComponent* Seed)
{
	if (!IsValid(Seed) || !ComponentCarriesPower(Seed))
	{
		return;
	}

	if (bBulkLoadDrainActive)
	{
		PromoteDirectHiddenLinks(Seed);
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

	FCircuitPromotionScope PromotionScope(this);

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
			TEXT("[PWR] mesh flood from %s promoted=%d skipped=%d visited=%d"),
			*Seed->GetName(),
			EdgesPromoted,
			EdgesSkipped,
			Visited.Num());
	}
}

void AStructuralPowerGraphSubsystem::PromoteDirectHiddenLinks(UFGPowerConnectionComponent* Seed)
{
	if (!IsValid(Seed) || !ComponentCarriesPower(Seed))
	{
		return;
	}

	FCircuitPromotionScope PromotionScope(this);

	TArray<UFGCircuitConnectionComponent*> HiddenLinks;
	Seed->GetHiddenConnections(HiddenLinks);

	for (UFGCircuitConnectionComponent* OtherRaw : HiddenLinks)
	{
		UFGPowerConnectionComponent* Other = Cast<UFGPowerConnectionComponent>(OtherRaw);
		if (!IsValid(Other))
		{
			continue;
		}

		PromoteCircuitLink(Seed, Other, ELogVerbosity::Verbose);
	}
}

void AStructuralPowerGraphSubsystem::PromoteOutletBusIfPowered(
	UFGStructuralPowerConnectionComponent* OutletBus,
	bool bLocalPromoteOnly)
{
	if (!IsValid(OutletBus))
	{
		return;
	}

	UFGStructuralPowerConnectionComponent* Seed = ComponentCarriesPower(OutletBus)
		? OutletBus
		: FindPoweredHiddenReachable(OutletBus);
	if (!Seed)
	{
		return;
	}

	if (bBulkLoadDrainActive || bLocalPromoteOnly)
	{
		PromoteDirectHiddenLinks(Seed);
	}
	else
	{
		PromoteStructuralMeshFrom(Seed);
	}
}

void AStructuralPowerGraphSubsystem::ApplyLocalBridgeBusAttach(
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
					if (IsValid(Visible) && ComponentCarriesPower(Visible))
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
						if (IsValid(Visible) && ComponentCarriesPower(Visible))
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

bool AStructuralPowerGraphSubsystem::TryMeshPeerBusOnComponent(
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

	RefreshBridgeEndpointRootIndex();

	const TArray<FStructuralNodeId>* EndpointsOnRoot = BridgeEndpointsByRoot.Find(Root);
	if (!EndpointsOnRoot)
	{
		return false;
	}

	for (const FStructuralNodeId& PeerId : *EndpointsOnRoot)
	{
		if (PeerId == SelfId)
		{
			continue;
		}

		const FTrackedEndpoint* PeerTracked = TrackedEndpoints.Find(PeerId);
		if (!PeerTracked)
		{
			continue;
		}

		AFGBuildable* SiblingHost = PeerTracked->Actor.Get();
		if (!IsValid(SiblingHost))
		{
			continue;
		}

		if (bBridgePeersOnly)
		{
			if (PeerTracked->Kind != EStructuralEndpointKind::Pole
				&& PeerTracked->Kind != EStructuralEndpointKind::Switch)
			{
				continue;
			}
		}
		else if (!ShouldMeshEndpoints(Host, SiblingHost, Root))
		{
			continue;
		}

		if (!ShouldEndpointParticipateInRestitch(SiblingHost, PeerTracked->Kind))
		{
			continue;
		}

		if (UFGStructuralPowerConnectionComponent* SiblingBus = FindBusConnector(SiblingHost))
		{
			return LinkHiddenPairLocal(OutletBus, SiblingBus);
		}
	}

	return false;
}

int32 AStructuralPowerGraphSubsystem::ResolveBridgeComponentRootBulk(
	AFGBuildable* Host,
	const FStructuralWallAnchor& Anchor,
	FStructuralNodeId& OutParentId)
{
	OutParentId = FStructuralNodeId();
	if (!IsValid(Host))
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

	const FBox Bounds = FStructuralAdjacencyHeuristics::GetActorAdjacencyBounds(Host);
	return StructureGraph.FindRootForBounds(Bounds, Host->GetClass(), &OutParentId);
}

int32 AStructuralPowerGraphSubsystem::ResolvePoleComponentRoot(
	AFGBuildablePowerPole* Pole,
	const FStructuralWallAnchor& Anchor,
	FStructuralNodeId& OutParentId)
{
	return ResolveBridgeComponentRootBulk(Pole, Anchor, OutParentId);
}

void AStructuralPowerGraphSubsystem::FinishBulkLoadDrain()
{
	if (!bBulkLoadDrainActive)
	{
		return;
	}

	bBulkLoadDrainActive = false;

	MarkBridgeEndpointRootIndexDirty();
	RefreshBridgeEndpointRootIndex();

	TSet<int32> Roots;
	for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : TrackedEndpoints)
	{
		if (Pair.Value.Kind != EStructuralEndpointKind::Pole
			&& Pair.Value.Kind != EStructuralEndpointKind::Switch)
		{
			continue;
		}

		const int32 Root = StructureGraph.FindRoot(Pair.Value.ParentId);
		if (Root != INDEX_NONE)
		{
			Roots.Add(Root);
		}
	}

	if (FStructuralPowerModConfig::IsPowerSwitchManualGroupsEnabled())
	{
		for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : TrackedEndpoints)
		{
			if (Pair.Value.Kind != EStructuralEndpointKind::Switch)
			{
				continue;
			}

			AFGBuildableCircuitSwitch* Switch =
				Cast<AFGBuildableCircuitSwitch>(Pair.Value.Actor.Get());
			if (!IsValid(Switch))
			{
				continue;
			}

			const int32 Root = StructureGraph.FindRoot(Pair.Value.ParentId);
			if (Root == INDEX_NONE || !Roots.Contains(Root))
			{
				continue;
			}

			const FName SwitchControl = ResolveControl(Switch, EStructuralChannel::Switch);
			RestitchSwitchKeyedConsumersOnRoot(Root, SwitchControl);
		}
	}

	UE_LOG(LogStructuralPower, Log,
		TEXT("[PWR] Post-load bulk drain complete — %d bridge component root(s)"),
		Roots.Num());
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

FStructuralComponentKey AStructuralPowerGraphSubsystem::MakeComponentKeyForBuildable(
	const AFGBuildable* Buildable) const
{
	if (!IsValid(Buildable))
	{
		return {};
	}

	if (const FTrackedEndpoint* Endpoint = TrackedEndpoints.Find(MakeNodeId(Buildable)))
	{
		return MakeComponentKeyForParent(Endpoint->ParentId);
	}

	const FStructuralWallAnchor Anchor = FStructuralAttachmentResolver::ResolveStructuralParent(
		const_cast<AFGBuildable*>(Buildable),
		GetWorld(),
		LightweightIndex);
	if (Anchor.IsValid())
	{
		return MakeComponentKeyForParent(MakeParentNodeId(Anchor));
	}

	return {};
}

bool AStructuralPowerGraphSubsystem::GetEndpointOverrides(
	const AFGBuildable* Buildable,
	FStructuralEndpointOverrides& Out) const
{
	Out = {};
	if (!IsValid(Buildable))
	{
		return false;
	}

	if (const FStructuralEndpointOverrides* Found = PlayerEndpointOverrides.Find(MakeNodeId(Buildable)))
	{
		Out = *Found;
		return Out.HasAnyOverride();
	}

	return false;
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

FName AStructuralPowerGraphSubsystem::ResolveSource(
	AFGBuildable* Buildable,
	EStructuralChannel Tag)
{
	if (!IsValid(Buildable) || Tag == EStructuralChannel::Structure)
	{
		return NAME_None;
	}

	if (FStructuralPowerRouter::UsesSourceControlModel(Tag))
	{
		if (const FStructuralEndpointOverrides* Overrides = PlayerEndpointOverrides.Find(MakeNodeId(Buildable));
			Overrides && !Overrides->SourceOverride.IsNone())
		{
			return Overrides->SourceOverride;
		}

		if (const FStructuralComponentKey ComponentKey = MakeComponentKeyForBuildable(Buildable);
			ComponentKey.IsValid())
		{
			return GetOrCreateComponentDefaultId(ComponentKey);
		}

		return NAME_None;
	}

	if (const FStructuralEndpointOverrides* Overrides = PlayerEndpointOverrides.Find(MakeNodeId(Buildable));
		Overrides && !Overrides->SourceOverride.IsNone())
	{
		return Overrides->SourceOverride;
	}

	if (Tag == EStructuralChannel::Switch)
	{
		if (const AFGBuildableCircuitSwitch* Switch = Cast<AFGBuildableCircuitSwitch>(Buildable))
		{
			const FName Control = FStructuralPowerRouter::ResolveSwitchControlFromTag(Switch);
			if (Control != StructuralPowerConstants::ControlBypass)
			{
				return Control;
			}
		}
	}

	if (const FStructuralComponentKey ComponentKey = MakeComponentKeyForBuildable(Buildable);
		ComponentKey.IsValid())
	{
		return GetOrCreateComponentDefaultId(ComponentKey);
	}

	return NAME_None;
}

FName AStructuralPowerGraphSubsystem::ResolveControl(
	AFGBuildable* Buildable,
	EStructuralChannel Tag)
{
	if (!IsValid(Buildable) || !FStructuralPowerRouter::UsesSourceControlModel(Tag))
	{
		return NAME_None;
	}

	if (const FStructuralEndpointOverrides* Overrides = PlayerEndpointOverrides.Find(MakeNodeId(Buildable));
		Overrides && !Overrides->ControlOverride.IsNone())
	{
		return Overrides->ControlOverride;
	}

	if (Tag == EStructuralChannel::Switch)
	{
		if (const AFGBuildableCircuitSwitch* Switch = Cast<AFGBuildableCircuitSwitch>(Buildable))
		{
			return FStructuralPowerRouter::ResolveSwitchControlFromTag(Switch);
		}

		return StructuralPowerConstants::ControlBypass;
	}

	if (Buildable->IsA<AFGBuildableLightsControlPanel>())
	{
		return StructuralPowerConstants::ControlUnconfigured;
	}

	return NAME_None;
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
	if (FStructuralPowerRouter::UsesSourceControlModel(Key.Tag))
	{
		Key.Source = ResolveSource(Buildable, Key.Tag);
		Key.Control = ResolveControl(Buildable, Key.Tag);
		Key.EffectiveId = Key.Source;
	}
	else
	{
		Key.EffectiveId = ResolveSource(Buildable, Key.Tag);
	}

	return Key;
}

int32 AStructuralPowerGraphSubsystem::GetEndpointComponentRoot(AFGBuildable* Endpoint)
{
	if (!IsValid(Endpoint))
	{
		return INDEX_NONE;
	}

	const FStructuralWallAnchor Anchor = ResolveOutletAnchor(Endpoint);
	FStructuralNodeId ParentId;
	return ResolveEndpointComponentRoot(Endpoint, Anchor, ParentId);
}

void AStructuralPowerGraphSubsystem::SetEndpointIds(
	AFGBuildable* Buildable,
	FName Source,
	FName Control,
	bool bClearSource,
	bool bClearControl)
{
	if (!IsValid(Buildable) || !Buildable->HasAuthority())
	{
		return;
	}

	const FStructuralNodeId NodeId = MakeNodeId(Buildable);
	FStructuralEndpointOverrides& Entry = PlayerEndpointOverrides.FindOrAdd(NodeId);

	if (bClearSource)
	{
		Entry.SourceOverride = NAME_None;
	}
	else if (FStructuralPowerRouter::IsPlayerChosenIdValid(Source))
	{
		Entry.SourceOverride = Source;
	}

	if (bClearControl)
	{
		Entry.ControlOverride = NAME_None;
	}
	else if (FStructuralPowerRouter::IsPlayerChosenIdValid(Control))
	{
		Entry.ControlOverride = Control;
	}

	if (!Entry.HasAnyOverride())
	{
		PlayerEndpointOverrides.Remove(NodeId);
	}

	const bool bIsLight = Buildable->IsA<AFGBuildableLightSource>();
	const bool bIsPanel = Buildable->IsA<AFGBuildableLightsControlPanel>();
	const bool bIsSwitch = Buildable->IsA<AFGBuildableCircuitSwitch>();
	const bool bGroupLighting = FStructuralPowerModConfig::IsGroupLightingEnabled();
	const bool bManualSwitchGroups = FStructuralPowerModConfig::IsPowerSwitchManualGroupsEnabled();

	const bool bSkipDeferredOutlet = (bGroupLighting && (bIsLight || bIsPanel))
		|| (bManualSwitchGroups && bIsSwitch);
	if (!bSkipDeferredOutlet)
	{
		EnqueuePlacement(
			Buildable,
			EStructuralPlacementJobType::Outlet,
			/*bDefer=*/true);
	}

	const FStructuralWallAnchor Anchor = ResolveOutletAnchor(Buildable);
	FStructuralNodeId ParentId;
	const int32 Root = ResolveEndpointComponentRoot(Buildable, Anchor, ParentId);
	if (Root == INDEX_NONE)
	{
		return;
	}

	if (bIsLight && bGroupLighting)
	{
		ProcessLightEndpoint(Cast<AFGBuildableLightSource>(Buildable));
	}
	else if (bIsPanel && bGroupLighting)
	{
		FTrackedEndpoint& Tracked = TrackedEndpoints.FindOrAdd(MakeNodeId(Buildable));
		Tracked.bPanelLinksReady = false;
		Tracked.bDownstreamLinksReady = false;
		ProcessPanelEndpoint(Cast<AFGBuildableLightsControlPanel>(Buildable));
	}
	else if (bIsSwitch && bManualSwitchGroups)
	{
		AFGBuildableCircuitSwitch* Switch = Cast<AFGBuildableCircuitSwitch>(Buildable);
		ProcessSwitchEndpoint(Switch);
		const FName SwitchControl = ResolveControl(Switch, EStructuralChannel::Switch);
		RestitchSwitchKeyedConsumersOnRoot(Root, SwitchControl);
	}
}

bool AStructuralPowerGraphSubsystem::CollectIdsOnComponent(
	const FStructuralComponentKey& Key,
	FStructuralComponentIdList& Out) const
{
	if (!Key.IsValid())
	{
		return false;
	}

	const int32 TargetRoot = StructureGraph.FindRoot(Key.CanonicalNodeId);
	if (TargetRoot == INDEX_NONE)
	{
		return false;
	}

	Out = {};
	Out.DefaultSourceId = const_cast<AStructuralPowerGraphSubsystem*>(this)
		->GetOrCreateComponentDefaultId(Key);

	TSet<FName> NamedSources;
	TSet<FName> NamedControls;
	TSet<FName> NamedSwitchControls;
	TSet<FName> NamedLightGroups;

	auto ConsiderBuildable = [&](const FStructuralNodeId& NodeId, const AFGBuildable* Buildable)
	{
		if (!IsValid(Buildable))
		{
			return;
		}

		const FStructuralComponentKey BuildableKey = MakeComponentKeyForBuildable(Buildable);
		if (!BuildableKey.IsValid()
			|| StructureGraph.FindRoot(BuildableKey.CanonicalNodeId) != TargetRoot)
		{
			return;
		}

		const bool bIsLight = FStructuralEligibilityRules::IsStructuralLightConsumer(Buildable);
		const bool bIsPanel = Buildable->IsA<AFGBuildableLightsControlPanel>();
		const bool bIsSwitch = Buildable->IsA<AFGBuildableCircuitSwitch>();

		if (const FStructuralEndpointOverrides* Overrides = PlayerEndpointOverrides.Find(NodeId))
		{
			if (!Overrides->SourceOverride.IsNone()
				&& Overrides->SourceOverride != Out.DefaultSourceId)
			{
				NamedSources.Add(Overrides->SourceOverride);
			}

			if (!Overrides->ControlOverride.IsNone()
				&& !FStructuralPowerRouter::IsReservedSentinel(Overrides->ControlOverride))
			{
				NamedControls.Add(Overrides->ControlOverride);
				if (bIsPanel)
				{
					NamedLightGroups.Add(Overrides->ControlOverride);
				}
				else if (bIsSwitch)
				{
					NamedSwitchControls.Add(Overrides->ControlOverride);
				}
			}
		}

		if (bIsSwitch)
		{
			const FName TagControl =
				FStructuralPowerRouter::ResolveSwitchControlFromTag(
					Cast<AFGBuildableCircuitSwitch>(Buildable));
			if (!FStructuralPowerRouter::IsReservedSentinel(TagControl))
			{
				NamedControls.Add(TagControl);
				NamedSwitchControls.Add(TagControl);
			}
		}

		const EStructuralChannel Tag = FStructuralEligibilityRules::ClassifyBuildable(Buildable);
		AStructuralPowerGraphSubsystem* MutableThis =
			const_cast<AStructuralPowerGraphSubsystem*>(this);
		AFGBuildable* MutableBuildable = const_cast<AFGBuildable*>(Buildable);
		const FName ResolvedSource = MutableThis->ResolveSource(MutableBuildable, Tag);
		const FName ResolvedControl = MutableThis->ResolveControl(MutableBuildable, Tag);

		if (FStructuralPowerRouter::IsPlayerChosenIdValid(ResolvedSource)
			&& ResolvedSource != Out.DefaultSourceId
			&& !bIsLight)
		{
			NamedSources.Add(ResolvedSource);
		}

		if (FStructuralPowerRouter::IsPlayerChosenIdValid(ResolvedControl)
			&& !FStructuralPowerRouter::IsReservedSentinel(ResolvedControl))
		{
			NamedControls.Add(ResolvedControl);
			if (bIsPanel)
			{
				NamedLightGroups.Add(ResolvedControl);
			}
			else if (bIsSwitch)
			{
				NamedSwitchControls.Add(ResolvedControl);
			}
		}
	};

	for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : TrackedEndpoints)
	{
		ConsiderBuildable(Pair.Key, Pair.Value.Actor.Get());
	}

	for (const TPair<FStructuralNodeId, TWeakObjectPtr<AFGBuildable>>& Pair : RegisteredBuildables)
	{
		ConsiderBuildable(Pair.Key, Pair.Value.Get());
	}

	for (const TPair<FStructuralNodeId, FStructuralEndpointOverrides>& Pair : PlayerEndpointOverrides)
	{
		if (const TWeakObjectPtr<AFGBuildable>* Buildable = RegisteredBuildables.Find(Pair.Key))
		{
			ConsiderBuildable(Pair.Key, Buildable->Get());
		}
	}

	Out.NamedControlIds = NamedControls.Array();
	Out.NamedSwitchControlIds = NamedSwitchControls.Array();
	Out.NamedLightGroupIds = NamedLightGroups.Array();

	for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : TrackedEndpoints)
	{
		if (Pair.Value.Kind != EStructuralEndpointKind::Light)
		{
			continue;
		}

		if (StructureGraph.FindRoot(Pair.Value.ParentId) != TargetRoot)
		{
			continue;
		}

		AFGBuildableLightSource* Light = Cast<AFGBuildableLightSource>(Pair.Value.Actor.Get());
		if (!IsValid(Light))
		{
			continue;
		}

		const FName LightSource = const_cast<AStructuralPowerGraphSubsystem*>(this)
			->ResolveSource(Light, EStructuralChannel::Light);
		if (!FStructuralPowerRouter::IsPlayerChosenIdValid(LightSource)
			|| LightSource == Out.DefaultSourceId)
		{
			continue;
		}

		if (NamedLightGroups.Contains(LightSource))
		{
			continue;
		}

		NamedSources.Add(LightSource);
	}

	for (const FName& LightGroupId : NamedLightGroups)
	{
		NamedSources.Remove(LightGroupId);
	}

	Out.NamedSourceIds = NamedSources.Array();
	Out.NamedSourceIds.Sort(FNameLexicalLess());
	Out.NamedControlIds.Sort(FNameLexicalLess());
	Out.NamedSwitchControlIds.Sort(FNameLexicalLess());
	Out.NamedLightGroupIds.Sort(FNameLexicalLess());
	return true;
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

int32 AStructuralPowerGraphSubsystem::ResolveBridgeHostComponentRoot(
	AFGBuildable* Host,
	FStructuralNodeId* OutParentId)
{
	if (!IsValid(Host))
	{
		if (OutParentId)
		{
			*OutParentId = FStructuralNodeId();
		}
		return INDEX_NONE;
	}

	FStructuralNodeId ParentId;
	const FStructuralWallAnchor Anchor = ResolveOutletAnchor(Host);
	const int32 Root = ResolveEndpointComponentRoot(Host, Anchor, ParentId);
	if (OutParentId)
	{
		*OutParentId = ParentId;
	}

	if (ParentId.IsValid())
	{
		if (FTrackedEndpoint* Tracked = TrackedEndpoints.Find(MakeNodeId(Host)))
		{
			Tracked->ParentId = ParentId;
		}
	}

	return Root;
}

void AStructuralPowerGraphSubsystem::MaybeRunPostLoadLightReconcile()
{
	if (GetPendingJobCount() > 0)
	{
		return;
	}

	if (bBulkLoadDrainActive)
	{
		FinishBulkLoadDrain();
	}

	if (!bPendingPostLoadLightReconcile)
	{
		return;
	}

	bPendingPostLoadLightReconcile = false;
	if (FStructuralPowerModConfig::IsGroupLightingEnabled())
	{
		ReconcileAllLightConsumers();
		ReconcileAllPanelEndpoints();
	}
}

void AStructuralPowerGraphSubsystem::ReconcileAllPanelEndpoints()
{
	if (!FStructuralPowerModConfig::IsGroupLightingEnabled())
	{
		return;
	}

	UWorld* World = GetWorld();
	AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(World);
	if (!IsValid(BuildableSubsystem))
	{
		return;
	}

	int32 PanelCount = 0;
	for (AFGBuildable* Buildable : BuildableSubsystem->GetAllBuildablesRef())
	{
		AFGBuildableLightsControlPanel* Panel = Cast<AFGBuildableLightsControlPanel>(Buildable);
		if (!IsValid(Panel) || !Panel->HasAuthority())
		{
			continue;
		}

		const FStructuralNodeId PanelId = MakeNodeId(Panel);
		FTrackedEndpoint& Tracked = TrackedEndpoints.FindOrAdd(PanelId);
		Tracked.bPanelLinksReady = false;
		Tracked.bDownstreamLinksReady = false;
		ProcessPanelEndpoint(Panel);
		++PanelCount;
	}

	UE_LOG(LogStructuralPower, Log,
		TEXT("[PWR] Post-load panel reconcile complete — %d panel(s)"),
		PanelCount);

	ApplyKeyedSubnetAllPanels();
}

void AStructuralPowerGraphSubsystem::ApplyKeyedSubnetAllPanels()
{
	if (!FStructuralPowerModConfig::IsGroupLightingEnabled())
	{
		return;
	}

	UWorld* World = GetWorld();
	AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(World);
	if (!IsValid(BuildableSubsystem))
	{
		return;
	}

	for (AFGBuildable* Buildable : BuildableSubsystem->GetAllBuildablesRef())
	{
		if (AFGBuildableLightsControlPanel* Panel = Cast<AFGBuildableLightsControlPanel>(Buildable))
		{
			if (IsValid(Panel) && Panel->HasAuthority())
			{
				FStructuralPanelControlledSync::ApplyKeyedSubnet(*this, Panel);
			}
		}
	}
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

void AStructuralPowerGraphSubsystem::LinkBusToVisibleConnectionsLocal(
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

void AStructuralPowerGraphSubsystem::LinkBusToVisibleConnections(
	AFGBuildable* Host,
	UFGStructuralPowerConnectionComponent* Bus)
{
	if (bBulkLoadDrainActive)
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

bool AStructuralPowerGraphSubsystem::HasBridgeBusPeerMesh(
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

UFGCircuitConnectionComponent* AStructuralPowerGraphSubsystem::GetComponentSourceConnector(
	int32 ComponentRoot,
	const AFGBuildable* ExcludeHost)
{
	if (ComponentRoot == INDEX_NONE)
	{
		return nullptr;
	}

	RefreshBridgeEndpointRootIndex();

	if (const TWeakObjectPtr<UFGCircuitConnectionComponent>* CachedEntry =
			SourceConnectorByRoot.Find(ComponentRoot))
	{
		UFGCircuitConnectionComponent* Cached = CachedEntry->Get();
		if (IsValid(Cached)
			&& Cached->GetOwner() != ExcludeHost
			&& ComponentCarriesPower(Cast<UFGPowerConnectionComponent>(Cached)))
		{
			return Cached;
		}
		SourceConnectorByRoot.Remove(ComponentRoot);
	}

	UFGPowerConnectionComponent* PoweredVisible = nullptr;
	UFGStructuralPowerConnectionComponent* PoweredBus = nullptr;

	const TArray<FStructuralNodeId>* EndpointsOnRoot = BridgeEndpointsByRoot.Find(ComponentRoot);
	if (!EndpointsOnRoot)
	{
		return nullptr;
	}

	for (const FStructuralNodeId& NodeId : *EndpointsOnRoot)
	{
		const FTrackedEndpoint* Tracked = TrackedEndpoints.Find(NodeId);
		if (!Tracked)
		{
			continue;
		}

		AFGBuildable* Host = Tracked->Actor.Get();
		if (!IsValid(Host) || Host == ExcludeHost)
		{
			continue;
		}

		// Switch outlet buses are torn down on toggle — never use them as feed donors.
		if (Tracked->Kind != EStructuralEndpointKind::Switch)
		{
			if (UFGStructuralPowerConnectionComponent* Bus = FindBusConnector(Host))
			{
				if (!PoweredBus && ComponentCarriesPower(Bus))
				{
					PoweredBus = Bus;
				}
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
		SourceConnectorByRoot.Add(ComponentRoot, PoweredVisible);
		return PoweredVisible;
	}

	if (PoweredBus)
	{
		SourceConnectorByRoot.Add(ComponentRoot, PoweredBus);
		return PoweredBus;
	}

	for (const FStructuralNodeId& NodeId : *EndpointsOnRoot)
	{
		const FTrackedEndpoint* Tracked = TrackedEndpoints.Find(NodeId);
		if (!Tracked)
		{
			continue;
		}

		AFGBuildable* Host = Tracked->Actor.Get();
		if (!IsValid(Host) || Host == ExcludeHost
			|| Tracked->Kind == EStructuralEndpointKind::Switch)
		{
			continue;
		}

		if (UFGStructuralPowerConnectionComponent* Bus = FindBusConnector(Host))
		{
			if (UFGStructuralPowerConnectionComponent* Reachable = FindPoweredHiddenReachable(Bus))
			{
				SourceConnectorByRoot.Add(ComponentRoot, Reachable);
				return Reachable;
			}
		}
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

UFGPowerConnectionComponent* AStructuralPowerGraphSubsystem::ResolveSubnetFeedConnector(
	int32 ComponentRoot,
	const FStructuralChannelKey& DeviceKey)
{
	if (ComponentRoot == INDEX_NONE || DeviceKey.Source.IsNone())
	{
		return nullptr;
	}

	const FStructuralComponentKey CompKey = MakeComponentKeyForRoot(ComponentRoot);
	if (!CompKey.IsValid())
	{
		return nullptr;
	}

	const FName DefaultId = GetOrCreateComponentDefaultId(CompKey);

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
		for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : TrackedEndpoints)
		{
			if (Pair.Value.Kind != EStructuralEndpointKind::Switch)
			{
				continue;
			}

			AFGBuildableCircuitSwitch* Switch =
				Cast<AFGBuildableCircuitSwitch>(Pair.Value.Actor.Get());
			if (!IsValid(Switch)
				|| StructureGraph.FindRoot(Pair.Value.ParentId) != ComponentRoot)
			{
				continue;
			}

			const FName SwitchControl = ResolveControl(Switch, EStructuralChannel::Switch);
			if (!FStructuralPowerRouter::ShouldRouteSwitchGate(
					SwitchControl,
					DeviceKey.Source,
					CompKey,
					CompKey))
			{
				continue;
			}

			if (!ShouldInjectSwitchStructuralPath(Switch))
			{
				continue;
			}

			if (UFGStructuralPowerConnectionComponent* Bus = FindBusConnector(Switch))
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
	if (!IsValid(Switch) || !Switch->IsBridgeActive())
	{
		return false;
	}

	return ShouldInjectSwitchStructuralPath(Switch);
}

bool AStructuralPowerGraphSubsystem::SwitchHasAssignedControl(
	const AFGBuildableCircuitSwitch* Switch) const
{
	if (!IsValid(Switch))
	{
		return false;
	}

	const FName Control = const_cast<AStructuralPowerGraphSubsystem*>(this)
		->ResolveControl(const_cast<AFGBuildableCircuitSwitch*>(Switch), EStructuralChannel::Switch);
	return Control != StructuralPowerConstants::ControlBypass && !Control.IsNone();
}

bool AStructuralPowerGraphSubsystem::SwitchNeedsAdvancedWork(
	const AFGBuildableCircuitSwitch* Switch) const
{
	return SwitchHasAssignedControl(Switch)
		|| FStructuralSwitchParentResolver::IsWiredToStructureSide(
			const_cast<AFGBuildableCircuitSwitch*>(Switch));
}

int32 AStructuralPowerGraphSubsystem::ResolveSwitchMountRoot(
	const FStructuralWallAnchor& Anchor,
	FStructuralNodeId& OutParentId)
{
	OutParentId = FStructuralNodeId();
	if (!Anchor.IsValid())
	{
		return INDEX_NONE;
	}

	OutParentId = MakeParentNodeId(Anchor);
	const int32 Root = StructureGraph.FindRoot(OutParentId);
	if (Root != INDEX_NONE)
	{
		return Root;
	}

	if (bBulkLoadDrainActive)
	{
		return INDEX_NONE;
	}

	if (EnsureParentRegisteredInGraph(Anchor, OutParentId))
	{
		return StructureGraph.FindRoot(OutParentId);
	}

	return INDEX_NONE;
}

bool AStructuralPowerGraphSubsystem::ShouldInjectSwitchStructuralPath(
	const AFGBuildableCircuitSwitch* Switch) const
{
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

	const bool bSwitchOnA = KeyA.Tag == EStructuralChannel::Switch;
	const FName SwitchControl = bSwitchOnA ? KeyA.Control : KeyB.Control;
	const FName DeviceSource = bSwitchOnA ? KeyB.Source : KeyA.Source;
	const EStructuralChannel PeerTag = bSwitchOnA ? KeyB.Tag : KeyA.Tag;

	// Import/export (DR-001): wired switch ON merges grid with default structure physical bus.
	if (PeerTag == EStructuralChannel::Structure)
	{
		return true;
	}

	return FStructuralPowerRouter::ShouldRouteSwitchGate(SwitchControl, DeviceSource, CompKey, CompKey);
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

void AStructuralPowerGraphSubsystem::EnsurePanelListener(AFGBuildableLightsControlPanel* Panel)
{
	if (!IsValid(Panel))
	{
		return;
	}

	TInlineComponentArray<UStructuralPowerPanelListener*> Listeners;
	Panel->GetComponents(Listeners);
	if (Listeners.Num() > 0)
	{
		return;
	}

	UStructuralPowerPanelListener* Listener =
		NewObject<UStructuralPowerPanelListener>(Panel, NAME_None, RF_Transient);
	if (!Listener)
	{
		return;
	}

	Panel->AddInstanceComponent(Listener);
	Listener->RegisterComponent();
	Listener->BindSubsystem(this, Panel);
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

	if (bBulkLoadDrainActive)
	{
		return;
	}

	const FStructuralNodeId SwitchId = MakeNodeId(Switch);
	if (FTrackedEndpoint* Tracked = TrackedEndpoints.Find(SwitchId))
	{
		FStructuralNodeId ParentId = Tracked->ParentId;
		int32 Root = FindRootForTrackedEndpoint(*Tracked);
		if (Root == INDEX_NONE)
		{
			const FStructuralSwitchParentResolveResult ParentResolve =
				FStructuralSwitchParentResolver::Resolve(
					Switch,
					GetWorld(),
					StructureGraph,
					LightweightIndex,
					/*bPreferWirePort=*/false);
			Root = ResolveSwitchMountRoot(ParentResolve.Anchor, ParentId);
			if (ParentResolve.IsValid())
			{
				Tracked->ParentId = ParentId;
				MarkBridgeEndpointRootIndexDirty();
			}
		}

		const FStructuralChannelKey SwitchKey = ResolveChannelKeyForBuildable(Switch);
		const bool bKeyedSubnet = SwitchHasAssignedControl(Switch);

		if (!Switch->IsBridgeActive())
		{
			TearDownSwitchStructuralLinks(Switch);
			if (Root != INDEX_NONE
				&& bKeyedSubnet
				&& FStructuralPowerModConfig::IsPowerSwitchManualGroupsEnabled())
			{
				RestitchSwitchKeyedConsumersOnRoot(Root, SwitchKey.Control);
			}
		}
		else
		{
			UFGStructuralPowerConnectionComponent* OutletBus = GetOrCreateBusConnector(Switch);
			if (OutletBus)
			{
				ApplySwitchRuntimeAttach(
					Switch,
					OutletBus,
					Root,
					SwitchId,
					/*bBulkLoad=*/false);
				if (Root != INDEX_NONE
					&& bKeyedSubnet
					&& FStructuralPowerModConfig::IsPowerSwitchManualGroupsEnabled())
				{
					RestitchSwitchKeyedConsumersOnRoot(Root, SwitchKey.Control);
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
		RestitchLightEndpointsForRoot(Root);
		RestitchPanelEndpointsForRoot(Root);
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

	if (bBulkLoadDrainActive)
	{
		if (FStructuralEligibilityRules::IsStructuralLightConsumer(Buildable))
		{
			return;
		}

		if (Buildable->IsA<AFGBuildableLightsControlPanel>())
		{
			return;
		}
	}

	if (FStructuralEligibilityRules::IsStructuralLightConsumer(Buildable))
	{
		ProcessLightEndpoint(Cast<AFGBuildableLightSource>(Buildable));
		return;
	}

	if (AFGBuildableLightsControlPanel* Panel = Cast<AFGBuildableLightsControlPanel>(Buildable))
	{
		if (bBulkLoadDrainActive)
		{
			return;
		}

		ProcessPanelEndpoint(Panel);
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

void AStructuralPowerGraphSubsystem::ProcessPoleWireDelta(AFGBuildablePowerPole* Pole)
{
	if (!IsValid(Pole))
	{
		return;
	}

	UFGStructuralPowerConnectionComponent* OutletBus = GetOrCreateBusConnector(Pole);
	if (!OutletBus)
	{
		return;
	}

	const FStructuralNodeId PoleId = MakeNodeId(Pole);
	FTrackedEndpoint& Tracked = TrackedEndpoints.FindOrAdd(PoleId);
	Tracked.Actor = Pole;
	Tracked.Kind = EStructuralEndpointKind::Pole;
	RegisterBuildableActor(Pole);

	if (!Tracked.ParentId.IsValid())
	{
		const FStructuralWallAnchor ParentAnchor = ResolveOutletAnchor(Pole);
		FStructuralNodeId ParentId;
		ResolvePoleComponentRoot(Pole, ParentAnchor, ParentId);
		Tracked.ParentId = ParentId;
		MarkBridgeEndpointRootIndexDirty();
	}

	const int32 Root = FindRootForTrackedEndpoint(Tracked);

	LinkBusToVisibleConnectionsLocal(Pole, OutletBus);

	if (Root != INDEX_NONE && !HasBridgeBusPeerMesh(OutletBus))
	{
		TryMeshPeerBusOnComponent(Pole, OutletBus, Root, PoleId, /*bBridgePeersOnly=*/true);
	}

	PromoteOutletBusIfPowered(OutletBus, /*bLocalPromoteOnly=*/true);

	UE_LOG(LogStructuralPower, Log,
		TEXT("[PWR] pole wire delta %s root=%d busCircuit=%d powered=%d"),
		*Pole->GetName(),
		Root,
		OutletBus->GetCircuitID(),
		ComponentCarriesPower(OutletBus) ? 1 : 0);
}

void AStructuralPowerGraphSubsystem::ProcessPoleEndpoint(AFGBuildablePowerPole* Pole)
{
	if (!IsValid(Pole))
	{
		return;
	}

	const bool bBulk = bBulkLoadDrainActive;
	const FStructuralNodeId PoleId = MakeNodeId(Pole);
	if (!bBulk)
	{
		if (const FTrackedEndpoint* Existing = TrackedEndpoints.Find(PoleId))
		{
			if (Existing->Kind == EStructuralEndpointKind::Pole && Existing->ParentId.IsValid())
			{
				ProcessPoleWireDelta(Pole);
				return;
			}
		}
	}

	UFGStructuralPowerConnectionComponent* OutletBus = GetOrCreateBusConnector(Pole);
	if (!OutletBus)
	{
		FStructuralPowerTrace::LogPlacementSkip(Pole, TEXT("outlet_bus_create_failed"));
		return;
	}

	const FStructuralWallAnchor ParentAnchor = ResolveOutletAnchor(Pole);
	FStructuralNodeId ParentId;
	const int32 Root = ResolvePoleComponentRoot(Pole, ParentAnchor, ParentId);

	FTrackedEndpoint& Tracked = TrackedEndpoints.FindOrAdd(PoleId);
	Tracked.Actor = Pole;
	Tracked.ParentId = ParentId;
	Tracked.Kind = EStructuralEndpointKind::Pole;
	RegisterBuildableActor(Pole);
	if (bBulk)
	{
		if (Root != INDEX_NONE && ParentId.IsValid())
		{
			AddBridgeEndpointToRootIndex(PoleId, Root);
		}
	}
	else
	{
		MarkBridgeEndpointRootIndexDirty();
	}

	LinkBusToVisibleConnections(Pole, OutletBus);

	if (Root != INDEX_NONE && (bBulk || !HasBridgeBusPeerMesh(OutletBus)))
	{
		TryMeshPeerBusOnComponent(Pole, OutletBus, Root, PoleId, bBulk);
	}

	PromoteOutletBusIfPowered(OutletBus, /*bLocalPromoteOnly=*/true);

	if (bBulk)
	{
		UE_LOG(LogStructuralPower, Verbose,
			TEXT("[PWR] outlet %s root=%d parentValid=%d busCircuit=%d powered=%d tag=%s id=%s"),
			*Pole->GetName(),
			Root,
			ParentAnchor.IsValid() ? 1 : 0,
			OutletBus->GetCircuitID(),
			ComponentCarriesPower(OutletBus) ? 1 : 0,
			StructuralChannelToString(EStructuralChannel::Structure),
			TEXT("-"));
	}
	else
	{
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
}

void AStructuralPowerGraphSubsystem::ApplySwitchBaseOutletAttach(
	AFGBuildableCircuitSwitch* Switch,
	UFGStructuralPowerConnectionComponent* OutletBus,
	int32 Root)
{
	if (!IsValid(Switch) || !IsValid(OutletBus) || Root == INDEX_NONE)
	{
		return;
	}

	// DR-001: bridge outlet bus to the switch's visible ports so structure power
	// can exit onto player wires — same as the advanced/bulk paths.
	LinkBusToVisibleConnectionsLocal(Switch, OutletBus);

	if (!DoesComponentRootCarryPower(Root))
	{
		return;
	}

	if (UFGCircuitConnectionComponent* Feed = GetComponentSourceConnector(Root, Switch))
	{
		if (UFGPowerConnectionComponent* FeedPower = Cast<UFGPowerConnectionComponent>(Feed))
		{
			LinkHiddenPairLocal(OutletBus, FeedPower);
		}
	}

	PromoteOutletBusIfPowered(OutletBus, /*bLocalPromoteOnly=*/true);
}

void AStructuralPowerGraphSubsystem::ApplySwitchRuntimeAttach(
	AFGBuildableCircuitSwitch* Switch,
	UFGStructuralPowerConnectionComponent* OutletBus,
	int32 Root,
	const FStructuralNodeId& SwitchId,
	bool bBulkLoad)
{
	if (!IsValid(Switch) || !IsValid(OutletBus))
	{
		return;
	}

	if (bBulkLoad)
	{
		LinkBusToVisibleConnections(Switch, OutletBus);
		if (Root != INDEX_NONE)
		{
			ApplyLocalBridgeBusAttach(Switch, OutletBus, Root, SwitchId, Switch);
		}
		else
		{
			PromoteOutletBusIfPowered(OutletBus, /*bLocalPromoteOnly=*/false);
		}
		return;
	}

	const bool bKeyedSubnet = SwitchHasAssignedControl(Switch);
	const bool bWiredBridge = FStructuralSwitchParentResolver::IsWiredToStructureSide(Switch);
	if (bKeyedSubnet || bWiredBridge)
	{
		ApplySwitchAdvancedAttach(Switch, OutletBus, Root, SwitchId, bKeyedSubnet);
	}
	else
	{
		ApplySwitchBaseOutletAttach(Switch, OutletBus, Root);
	}
}

void AStructuralPowerGraphSubsystem::ApplySwitchAdvancedAttach(
	AFGBuildableCircuitSwitch* Switch,
	UFGStructuralPowerConnectionComponent* OutletBus,
	int32 Root,
	const FStructuralNodeId& SwitchId,
	bool bKeyedSubnet)
{
	if (!IsValid(Switch) || !IsValid(OutletBus))
	{
		return;
	}

	LinkBusToVisibleConnectionsLocal(Switch, OutletBus);

	if (Root == INDEX_NONE)
	{
		PromoteOutletBusIfPowered(OutletBus, /*bLocalPromoteOnly=*/true);
		return;
	}

	UFGPowerConnectionComponent* FeedPower = nullptr;
	if (UFGCircuitConnectionComponent* Conn0 = Switch->GetConnection0())
	{
		if (UFGPowerConnectionComponent* Visible = Cast<UFGPowerConnectionComponent>(Conn0))
		{
			if (IsValid(Visible) && ComponentCarriesPower(Visible))
			{
				FeedPower = Visible;
			}
		}
	}
	if (!FeedPower)
	{
		if (UFGCircuitConnectionComponent* Conn1 = Switch->GetConnection1())
		{
			if (UFGPowerConnectionComponent* Visible = Cast<UFGPowerConnectionComponent>(Conn1))
			{
				if (IsValid(Visible) && ComponentCarriesPower(Visible))
				{
					FeedPower = Visible;
				}
			}
		}
	}
	if (!FeedPower)
	{
		if (UFGCircuitConnectionComponent* Feed =
				GetComponentSourceConnector(Root, Switch))
		{
			FeedPower = Cast<UFGPowerConnectionComponent>(Feed);
		}
	}

	if (FeedPower)
	{
		LinkHiddenPairLocal(OutletBus, FeedPower);
	}

	if (bKeyedSubnet && !HasBridgeBusPeerMesh(OutletBus))
	{
		TryMeshPeerBusOnComponent(
			Switch,
			OutletBus,
			Root,
			SwitchId,
			/*bBridgePeersOnly=*/false);
	}

	PromoteOutletBusIfPowered(OutletBus, /*bLocalPromoteOnly=*/true);
}

void AStructuralPowerGraphSubsystem::RegisterSwitchOutletBase(
	AFGBuildableCircuitSwitch* Switch,
	const FStructuralWallAnchor& ParentAnchor,
	FTrackedEndpoint& InOutTracked,
	int32& OutRoot,
	FStructuralNodeId& OutParentId)
{
	OutRoot = ResolveSwitchMountRoot(ParentAnchor, OutParentId);
	InOutTracked.Actor = Switch;
	InOutTracked.ParentId = OutParentId;
	InOutTracked.Kind = EStructuralEndpointKind::Switch;
	RegisterBuildableActor(Switch);
	if (OutParentId.IsValid() && OutRoot != INDEX_NONE)
	{
		if (bBulkLoadDrainActive)
		{
			AddBridgeEndpointToRootIndex(MakeNodeId(Switch), OutRoot);
		}
		else
		{
			MarkBridgeEndpointRootIndexDirty();
		}
	}
	EnsureSwitchListener(Switch);
}

void AStructuralPowerGraphSubsystem::ProcessSwitchEndpoint(AFGBuildableCircuitSwitch* Switch)
{
	if (!IsValid(Switch) || !FStructuralPowerModConfig::IsGatePowerSwitchesEnabled())
	{
		FStructuralPowerTrace::LogPlacementSkip(Switch, TEXT("switch_gating_disabled"));
		return;
	}

	const bool bBulk = bBulkLoadDrainActive;
	const FStructuralSwitchParentResolveResult ParentResolve =
		FStructuralSwitchParentResolver::Resolve(
			Switch,
			GetWorld(),
			StructureGraph,
			LightweightIndex);
	const FStructuralWallAnchor ParentAnchor = ParentResolve.Anchor;

	const FStructuralNodeId SwitchId = MakeNodeId(Switch);
	FTrackedEndpoint& Tracked = TrackedEndpoints.FindOrAdd(SwitchId);
	FStructuralNodeId ParentId;
	int32 Root = INDEX_NONE;
	RegisterSwitchOutletBase(Switch, ParentResolve.Anchor, Tracked, Root, ParentId);

	const FStructuralChannelKey ChannelKey = ResolveChannelKeyForBuildable(Switch);
	const TCHAR* WirePort = ParentResolve.WirePortIndex == 0
		? TEXT("A")
		: (ParentResolve.WirePortIndex == 1 ? TEXT("B") : TEXT("-"));

	auto LogSwitchOutlet = [&](UFGStructuralPowerConnectionComponent* OutletBus, int32 Powered, const TCHAR* Mode)
	{
		UE_LOG(LogStructuralPower, Log,
			TEXT("[PWR] outlet %s root=%d parentValid=%d busCircuit=%d powered=%d tag=%s"
				" source=%s control=%s wirePort=%s mode=%s"),
			*Switch->GetName(),
			Root,
			ParentAnchor.IsValid() ? 1 : 0,
			IsValid(OutletBus) ? OutletBus->GetCircuitID() : INDEX_NONE,
			Powered,
			StructuralChannelToString(ChannelKey.Tag),
			*FStructuralPowerTrace::FormatSourceForTrace(ChannelKey),
			*FStructuralPowerTrace::FormatControlForTrace(ChannelKey),
			WirePort,
			Mode);
	};

	if (!Switch->IsBridgeActive())
	{
		TearDownSwitchStructuralLinks(Switch);
		LogSwitchOutlet(nullptr, 0, TEXT("inactive"));
		return;
	}

	UFGStructuralPowerConnectionComponent* OutletBus = GetOrCreateBusConnector(Switch);
	if (!OutletBus)
	{
		FStructuralPowerTrace::LogPlacementSkip(Switch, TEXT("switch_bus_create_failed"));
		return;
	}

	const bool bKeyedSubnet = SwitchHasAssignedControl(Switch);
	const bool bWiredBridge = FStructuralSwitchParentResolver::IsWiredToStructureSide(Switch);
	ApplySwitchRuntimeAttach(Switch, OutletBus, Root, SwitchId, bBulk);

	LogSwitchOutlet(
		OutletBus,
		ComponentCarriesPower(OutletBus) ? 1 : 0,
		bBulk ? TEXT("bulk")
			: (bKeyedSubnet ? TEXT("keyed") : (bWiredBridge ? TEXT("wired") : TEXT("base"))));

	if (bBulk)
	{
		return;
	}

	if (Root != INDEX_NONE
		&& bKeyedSubnet
		&& FStructuralPowerModConfig::IsPowerSwitchManualGroupsEnabled())
	{
		RestitchSwitchKeyedConsumersOnRoot(Root, ChannelKey.Control);
	}
}

void AStructuralPowerGraphSubsystem::TearDownLightStructuralLinks(AFGBuildableLightSource* Light)
{
	if (!IsValid(Light))
	{
		return;
	}

	if (UFGPowerConnectionComponent* Plug = FStructuralDeviceAttach::FindLightWireConnection(Light))
	{
		FStructuralDeviceAttach::TearDownConsumerLinks(Plug);
	}
}

void AStructuralPowerGraphSubsystem::ProcessLightEndpoint(AFGBuildableLightSource* Light)
{
	if (!IsValid(Light))
	{
		return;
	}

	const FStructuralChannelKey ChannelKey = ResolveChannelKeyForBuildable(Light);
	const FStructuralWallAnchor ParentAnchor = ResolveOutletAnchor(Light);
	FStructuralNodeId ParentId;
	const int32 Root = ResolveEndpointComponentRoot(Light, ParentAnchor, ParentId);

	const FStructuralNodeId LightId = MakeNodeId(Light);
	FTrackedEndpoint& Tracked = TrackedEndpoints.FindOrAdd(LightId);
	Tracked.Actor = Light;
	Tracked.ParentId = ParentId;
	Tracked.Kind = EStructuralEndpointKind::Light;
	RegisterBuildableActor(Light);

	UFGPowerConnectionComponent* Plug = FStructuralDeviceAttach::FindLightWireConnection(Light);
	if (!IsValid(Plug))
	{
		FStructuralPowerTrace::LogPlacementSkip(Light, TEXT("light_plug_missing"));
		return;
	}

	FStructuralDeviceAttach::TearDownConsumerLinks(Plug);

	auto LogLightOutlet = [&](int32 Powered, int32 BusCircuit)
	{
		UE_LOG(LogStructuralPower, Log,
			TEXT("[PWR] light %s root=%d parentValid=%d busCircuit=%d powered=%d tag=%s"
				" source=%s control=%s wirePort=-"),
			*Light->GetName(),
			Root,
			ParentAnchor.IsValid() ? 1 : 0,
			BusCircuit,
			Powered,
			StructuralChannelToString(ChannelKey.Tag),
			*FStructuralPowerTrace::FormatSourceForTrace(ChannelKey),
			*FStructuralPowerTrace::FormatControlForTrace(ChannelKey));
	};

	if (!FStructuralPowerModConfig::IsGroupLightingEnabled())
	{
		LogLightOutlet(0, INDEX_NONE);
		return;
	}

	if (Root == INDEX_NONE || !DoesComponentRootCarryPower(Root))
	{
		FStructuralPowerTrace::LogPlacementSkip(Light, TEXT("light_no_component_feed"));
		LogLightOutlet(0, INDEX_NONE);
		return;
	}

	const bool bAttached = FStructuralDeviceAttach::TryAttachConsumer(
		*this,
		Light,
		Plug,
		Root,
		ChannelKey);
	if (!bAttached)
	{
		FStructuralPowerTrace::LogPlacementSkip(Light, TEXT("light_attach_failed"));
	}
	if (bAttached)
	{
		PromoteStructuralMeshFrom(Plug);
	}

	const int32 BusCircuit = Plug->GetCircuitID();
	const int32 Powered = ConnectorSuppliesPower(Plug) ? 1 : 0;
	LogLightOutlet(Powered, BusCircuit);

	if (Root != INDEX_NONE && !ChannelKey.Source.IsNone())
	{
		RestitchPanelsWithControlOnRoot(Root, ChannelKey.Source);
	}
}

void AStructuralPowerGraphSubsystem::TearDownPanelStructuralLinks(
	AFGBuildableLightsControlPanel* Panel)
{
	if (!IsValid(Panel))
	{
		return;
	}

	FStructuralPanelPorts Ports;
	if (FStructuralPanelPortResolver::Resolve(Panel, Ports))
	{
		FStructuralPanelAttach::TearDownLinks(Panel, Ports);
	}
}

void AStructuralPowerGraphSubsystem::ProcessPanelEndpoint(AFGBuildableLightsControlPanel* Panel)
{
	if (!IsValid(Panel))
	{
		return;
	}

	FCircuitPromotionScope PromotionScope(this);

	FStructuralPanelPorts Ports;
	if (!FStructuralPanelPortResolver::Resolve(Panel, Ports))
	{
		FStructuralPowerTrace::LogPlacementSkip(Panel, TEXT("panel_ports_unresolved"));
		return;
	}

	const FStructuralChannelKey ChannelKey = ResolveChannelKeyForBuildable(Panel);
	const FStructuralWallAnchor ParentAnchor = ResolveOutletAnchor(Panel);
	FStructuralNodeId ParentId;
	const int32 Root = ResolveEndpointComponentRoot(Panel, ParentAnchor, ParentId);

	const FStructuralNodeId PanelId = MakeNodeId(Panel);
	FTrackedEndpoint& Tracked = TrackedEndpoints.FindOrAdd(PanelId);
	const bool bRoutingUnchanged = Tracked.bPanelLinksReady
		&& Tracked.CachedPanelRoot == Root
		&& Tracked.CachedPanelKey == ChannelKey;

	Tracked.Actor = Panel;
	Tracked.ParentId = ParentId;
	Tracked.Kind = EStructuralEndpointKind::Panel;
	RegisterBuildableActor(Panel);
	EnsurePanelListener(Panel);

	auto LogPanelOutlet = [&](int32 Powered, int32 BusCircuit)
	{
		UE_LOG(LogStructuralPower, Verbose,
			TEXT("[PWR] panel %s root=%d parentValid=%d busCircuit=%d powered=%d tag=%s"
				" source=%s control=%s wirePort=-"),
			*Panel->GetName(),
			Root,
			ParentAnchor.IsValid() ? 1 : 0,
			BusCircuit,
			Powered,
			StructuralChannelToString(ChannelKey.Tag),
			*FStructuralPowerTrace::FormatSourceForTrace(ChannelKey),
			*FStructuralPowerTrace::FormatControlForTrace(ChannelKey));
	};

	if (!FStructuralPowerModConfig::IsGroupLightingEnabled())
	{
		if (!bRoutingUnchanged)
		{
			FStructuralPanelAttach::TearDownLinks(Panel, Ports);
			Tracked.bPanelLinksReady = false;
		}

		LogPanelOutlet(0, INDEX_NONE);
		return;
	}

	if (bRoutingUnchanged)
	{
		const UFGPowerConnectionComponent* InputPower =
			FStructuralPanelPortResolver::AsPowerConnection(Ports.Input);
		const int32 BusCircuit = IsValid(InputPower) ? InputPower->GetCircuitID() : INDEX_NONE;
		const int32 Powered = ConnectorSuppliesPower(InputPower) ? 1 : 0;
		LogPanelOutlet(Powered, BusCircuit);

		const bool bDownstreamUnchanged = Tracked.bDownstreamLinksReady
			&& Tracked.CachedDownstreamControl == ChannelKey.Control;
		if (Root != INDEX_NONE
			&& ChannelKey.Control != StructuralPowerConstants::ControlUnconfigured)
		{
			if (!bDownstreamUnchanged)
			{
				FStructuralPanelAttach::RestitchDownstream(
					*this,
					Panel,
					Ports,
					Root,
					ChannelKey.Control);
				Tracked.bDownstreamLinksReady = true;
				Tracked.CachedDownstreamControl = ChannelKey.Control;
			}
			else
			{
				FStructuralPanelControlledSync::ApplyKeyedSubnet(*this, Panel);
			}
		}

		return;
	}

	FStructuralPanelAttach::TearDownLinks(Panel, Ports);
	Tracked.bDownstreamLinksReady = false;
	Tracked.CachedDownstreamControl = NAME_None;

	UFGPowerConnectionComponent* InputPower =
		FStructuralPanelPortResolver::AsPowerConnection(Ports.Input);
	const bool bInputWasPowered = ConnectorSuppliesPower(InputPower);

	bool bSupplyReady = false;
	if (Root != INDEX_NONE && DoesComponentRootCarryPower(Root))
	{
		if (FStructuralPanelAttach::SupplyAlreadyLinked(*this, Panel, Ports, Root, ChannelKey))
		{
			bSupplyReady = true;
		}
		else
		{
			bSupplyReady = FStructuralPanelAttach::TryLinkSupply(
				*this,
				Panel,
				Ports,
				Root,
				ChannelKey);
		}
	}

	if (bSupplyReady && !bInputWasPowered && IsValid(InputPower))
	{
		PromoteStructuralMeshFrom(InputPower);
	}

	if (Root != INDEX_NONE
		&& ChannelKey.Control != StructuralPowerConstants::ControlUnconfigured)
	{
		const bool bDownstreamUnchanged = Tracked.bDownstreamLinksReady
			&& Tracked.CachedDownstreamControl == ChannelKey.Control;
		if (!bDownstreamUnchanged)
		{
			FStructuralPanelAttach::RestitchDownstream(
				*this,
				Panel,
				Ports,
				Root,
				ChannelKey.Control);
		}
		else
		{
			FStructuralPanelControlledSync::ApplyKeyedSubnet(*this, Panel);
		}

		Tracked.bDownstreamLinksReady = true;
		Tracked.CachedDownstreamControl = ChannelKey.Control;
	}
	else
	{
		Tracked.bDownstreamLinksReady = false;
		Tracked.CachedDownstreamControl = NAME_None;
	}

	Tracked.CachedPanelKey = ChannelKey;
	Tracked.CachedPanelRoot = Root;
	Tracked.bPanelLinksReady = true;

	const int32 BusCircuit = IsValid(InputPower) ? InputPower->GetCircuitID() : INDEX_NONE;
	const int32 Powered = ConnectorSuppliesPower(InputPower) ? 1 : 0;
	const int32 Controlled = Panel->GetControlledBuildables(AFGBuildableLightSource::StaticClass()).Num();
	UE_LOG(LogStructuralPower, Verbose,
		TEXT("[PWR] panel %s root=%d powered=%d busCircuit=%d source=%s control=%s controlled=%d"),
		*Panel->GetName(),
		Root,
		Powered,
		BusCircuit,
		*FStructuralPowerTrace::FormatSourceForTrace(ChannelKey),
		*FStructuralPowerTrace::FormatControlForTrace(ChannelKey),
		Controlled);
}

void AStructuralPowerGraphSubsystem::RestitchPanelEndpointsForRoot(int32 Root)
{
	if (Root == INDEX_NONE || !FStructuralPowerModConfig::IsGroupLightingEnabled())
	{
		return;
	}

	for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : TrackedEndpoints)
	{
		if (Pair.Value.Kind != EStructuralEndpointKind::Panel)
		{
			continue;
		}

		if (StructureGraph.FindRoot(Pair.Value.ParentId) != Root)
		{
			continue;
		}

		if (AFGBuildableLightsControlPanel* Panel =
				Cast<AFGBuildableLightsControlPanel>(Pair.Value.Actor.Get()))
		{
			FTrackedEndpoint& Tracked = TrackedEndpoints.FindOrAdd(Pair.Key);
			Tracked.bPanelLinksReady = false;
			Tracked.bDownstreamLinksReady = false;
			ProcessPanelEndpoint(Panel);
		}
	}
}

void AStructuralPowerGraphSubsystem::RestitchPanelsWithControlOnRoot(int32 Root, FName ControlId)
{
	if (Root == INDEX_NONE
		|| !FStructuralPowerModConfig::IsGroupLightingEnabled()
		|| ControlId.IsNone()
		|| ControlId == StructuralPowerConstants::ControlUnconfigured)
	{
		return;
	}

	for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : TrackedEndpoints)
	{
		if (Pair.Value.Kind != EStructuralEndpointKind::Panel)
		{
			continue;
		}

		if (StructureGraph.FindRoot(Pair.Value.ParentId) != Root)
		{
			continue;
		}

		AFGBuildableLightsControlPanel* Panel =
			Cast<AFGBuildableLightsControlPanel>(Pair.Value.Actor.Get());
		if (!IsValid(Panel))
		{
			continue;
		}

		const FStructuralChannelKey PanelKey = ResolveChannelKeyForBuildable(Panel);
		if (PanelKey.Control != ControlId)
		{
			continue;
		}

		FTrackedEndpoint& Tracked = TrackedEndpoints.FindOrAdd(Pair.Key);
		Tracked.bPanelLinksReady = false;
		Tracked.bDownstreamLinksReady = false;
		ProcessPanelEndpoint(Panel);
	}
}

void AStructuralPowerGraphSubsystem::RestitchSwitchKeyedConsumersOnRoot(
	int32 Root,
	FName SwitchControlId)
{
	if (Root == INDEX_NONE
		|| SwitchControlId.IsNone()
		|| SwitchControlId == StructuralPowerConstants::ControlBypass)
	{
		return;
	}

	for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : TrackedEndpoints)
	{
		if (StructureGraph.FindRoot(Pair.Value.ParentId) != Root)
		{
			continue;
		}

		AFGBuildable* Host = Pair.Value.Actor.Get();
		if (!IsValid(Host))
		{
			continue;
		}

		const FName HostSource = ResolveSource(Host, EStructuralChannel::Light);

		if (Pair.Value.Kind == EStructuralEndpointKind::Panel)
		{
			if (HostSource != SwitchControlId)
			{
				continue;
			}

			if (AFGBuildableLightsControlPanel* Panel = Cast<AFGBuildableLightsControlPanel>(Host))
			{
				FTrackedEndpoint& Tracked = TrackedEndpoints.FindOrAdd(Pair.Key);
				Tracked.bPanelLinksReady = false;
				Tracked.bDownstreamLinksReady = false;
				ProcessPanelEndpoint(Panel);
			}
		}
		else if (Pair.Value.Kind == EStructuralEndpointKind::Light
			&& FStructuralPowerModConfig::IsGroupLightingEnabled())
		{
			if (HostSource != SwitchControlId)
			{
				continue;
			}

			if (AFGBuildableLightSource* Light = Cast<AFGBuildableLightSource>(Host))
			{
				ProcessLightEndpoint(Light);
			}
		}
	}
}

void AStructuralPowerGraphSubsystem::RestitchLightEndpointsForRoot(int32 Root)
{
	if (Root == INDEX_NONE || !FStructuralPowerModConfig::IsGroupLightingEnabled())
	{
		return;
	}

	for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : TrackedEndpoints)
	{
		if (Pair.Value.Kind != EStructuralEndpointKind::Light)
		{
			continue;
		}

		if (StructureGraph.FindRoot(Pair.Value.ParentId) != Root)
		{
			continue;
		}

		if (AFGBuildableLightSource* Light = Cast<AFGBuildableLightSource>(Pair.Value.Actor.Get()))
		{
			ProcessLightEndpoint(Light);
		}
	}
}

void AStructuralPowerGraphSubsystem::EnumerateTrackedLightsOnRoot(
	int32 Root,
	TFunctionRef<void(AFGBuildableLightSource*)> Visitor) const
{
	if (Root == INDEX_NONE)
	{
		return;
	}

	for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : TrackedEndpoints)
	{
		if (Pair.Value.Kind != EStructuralEndpointKind::Light)
		{
			continue;
		}

		if (StructureGraph.FindRoot(Pair.Value.ParentId) != Root)
		{
			continue;
		}

		if (AFGBuildableLightSource* Light = Cast<AFGBuildableLightSource>(Pair.Value.Actor.Get()))
		{
			Visitor(Light);
		}
	}
}

void AStructuralPowerGraphSubsystem::ReconcileAllLightConsumers()
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	TArray<AFGBuildableLightSource*> Lights;
	TSet<AFGBuildableLightSource*> Seen;

	auto Consider = [&](AFGBuildableLightSource* Light)
	{
		if (!IsValid(Light) || Seen.Contains(Light))
		{
			return;
		}

		Seen.Add(Light);
		Lights.Add(Light);
	};

	for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : TrackedEndpoints)
	{
		if (Pair.Value.Kind == EStructuralEndpointKind::Light)
		{
			Consider(Cast<AFGBuildableLightSource>(Pair.Value.Actor.Get()));
		}
	}

	if (AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(World))
	{
		for (AFGBuildable* Buildable : BuildableSubsystem->GetAllBuildablesRef())
		{
			if (FStructuralEligibilityRules::IsStructuralLightConsumer(Buildable))
			{
				Consider(Cast<AFGBuildableLightSource>(Buildable));
			}
		}
	}

	UE_LOG(LogStructuralPower, Log,
		TEXT("Reconcile lights — GroupLighting=%d candidate(s)=%d"),
		FStructuralPowerModConfig::IsGroupLightingEnabled() ? 1 : 0,
		Lights.Num());

	for (AFGBuildableLightSource* Light : Lights)
	{
		ProcessLightEndpoint(Light);
	}
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

	FStructuralPowerTrace::LogHook(Pole, TEXT("OnPowerConnectionChanged"), TEXT("wire_refresh"), TEXT("pole_wire_delta"));
	ProcessPoleWireDelta(Pole);
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
			else if (Tracked->Kind == EStructuralEndpointKind::Light)
			{
				TearDownLightStructuralLinks(Cast<AFGBuildableLightSource>(Host));
			}
			else if (Tracked->Kind == EStructuralEndpointKind::Panel)
			{
				TearDownPanelStructuralLinks(Cast<AFGBuildableLightsControlPanel>(Host));
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
		MarkBridgeEndpointRootIndexDirty();

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
