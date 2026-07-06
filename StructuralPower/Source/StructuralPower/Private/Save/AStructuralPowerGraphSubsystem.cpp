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
#include "Circuit/FStructuralCircuitPromotionScope.h"
#include "Circuit/FStructuralCircuitPromotionUtil.h"
#include "Circuit/FStructuralHiddenConnectionUtil.h"
#include "Components/UFGStructuralPowerConnectionComponent.h"
#include "Config/FStructuralPowerModConfig.h"
#include "Diagnostics/FStructuralPowerDiagnostics.h"
#include "Diagnostics/FStructuralPowerTrace.h"
#include "Equipment/FStructuralEquipmentBridgeRegistry.h"
#include "Engine/World.h"
#include "FGBuildableSubsystem.h"
#include "FGCircuitConnectionComponent.h"
#include "FGCircuitSubsystem.h"
#include "FGLightweightBuildableSubsystem.h"
#include "FGPowerConnectionComponent.h"
#include "Graph/FStructuralAdjacencyHeuristics.h"
#include "Graph/FStructuralAttachmentResolver.h"
#include "Graph/FStructuralOutletParentResolver.h"
#include "Graph/FStructuralEndpointIndex.h"
#include "Graph/FStructuralBusMemberSpatialIndex.h"
#include "Routing/FStructuralPowerRouter.h"
#include "Kismet/GameplayStatics.h"
#include "Lightweight/FStructuralLightweightIndex.h"
#include "Graph/FStructuralSwitchParentResolver.h"
#include "Network/UStructuralPowerSwitchListener.h"
#include "Network/UStructuralPowerPanelListener.h"
#include "Panel/FStructuralPanelControlledSync.h"
#include "Panel/FStructuralPanelPortResolver.h"
#include "Processors/FStructuralPowerLightProcessor.h"
#include "Processors/FStructuralPowerPanelProcessor.h"
#include "Processors/FStructuralPowerSwitchProcessor.h"
#include "Reconcile/FStructuralSiteBusMesh.h"
#include "Rules/FStructuralEligibilityRules.h"
#include "Session/FStructuralPowerSessionSettings.h"
#include "StructuralPowerConstants.h"
#include "StructuralPowerLog.h"
#include "HAL/PlatformTime.h"

AStructuralPowerGraphSubsystem::AStructuralPowerGraphSubsystem()
{
	bReplicates = false;
	IdRegistry.Bind(ComponentDefaultIds, PlayerEndpointOverrides);
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
	return FStructuralAttachmentResolver::ResolveStructuralParent(
		Outlet,
		GetWorld(),
		MakeOutletParentResolveParams());
}

FStructuralOutletParentResolveParams AStructuralPowerGraphSubsystem::MakeOutletParentResolveParams() const
{
	FStructuralOutletParentResolveParams Params;
	Params.LightweightIndex = &LightweightIndex;
	Params.BusMemberIndex = &BusMemberSpatialIndex;
	Params.StructureGraph = &StructureGraph;
	Params.bAllowLiveScan = !bBulkLoadDrainActive;
	Params.ResolveActorFromNodeId = [this](const FStructuralNodeId& NodeId) -> AFGBuildable*
	{
		if (const TWeakObjectPtr<AFGBuildable>* Entry = RegisteredBuildables.Find(NodeId))
		{
			return Entry->Get();
		}
		return nullptr;
	};
	return Params;
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

	CrossSiteGraph.Clear();

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
		EndpointIndex.Reset();
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
		PlacementQueue.EnqueueLightweight(Key);
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
		PlacementQueue.EnqueueBuildable(Buildable, JobType);
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

bool AStructuralPowerGraphSubsystem::IsBuildablePlacementPending(AFGBuildable* Buildable) const
{
	if (!IsValid(Buildable))
	{
		return false;
	}

	return PlacementQueue.IsAnyBuildableJobPending(Buildable);
}

void AStructuralPowerGraphSubsystem::TickDeferredPlacements(int32 MaxJobs)
{
	if (!FStructuralPowerSessionSettings::IsPropagationEnabled())
	{
		return;
	}

	PlacementQueue.Tick(
		MaxJobs,
		bBulkLoadDrainActive,
		[this](AFGBuildable* Buildable, EStructuralPlacementJobType JobType)
		{
			if (JobType == EStructuralPlacementJobType::Outlet)
			{
				ProcessOutlet(Buildable);
			}
			else
			{
				ProcessStructure(Buildable);
			}
		},
		[this](const FStructuralLightweightKey& Key)
		{
			ProcessLightweightStructure(Key);
		},
		[this]()
		{
			MaybeRunPostLoadLightReconcile();
		});
}

void AStructuralPowerGraphSubsystem::BeginCircuitPromotion()
{
	++CircuitPromotionDepth;
}

void AStructuralPowerGraphSubsystem::EndCircuitPromotion()
{
	check(CircuitPromotionDepth > 0);
	--CircuitPromotionDepth;
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
		FStructuralCircuitPromotionScope PromotionScope(this);
		FStructuralCircuitPromotionUtil::PromoteCircuitLink(A, B, ELogVerbosity::Verbose);
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
		FStructuralCircuitPromotionScope PromotionScope(this);
		FStructuralCircuitPromotionUtil::PromoteCircuitLink(A, B);
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
		FStructuralCircuitPromotionScope PromotionScope(this);
		FStructuralCircuitPromotionUtil::PromoteCircuitLink(A, B, ELogVerbosity::Verbose);
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

	EndpointIndex.RebuildFrom(TrackedEndpoints, StructureGraph);
	bBridgeEndpointRootIndexDirty = false;
}

void AStructuralPowerGraphSubsystem::AddEndpointToRootIndex(
	int32 Root,
	EStructuralEndpointKind Kind,
	const FStructuralNodeId& EndpointId)
{
	if (Root == INDEX_NONE || !EndpointId.IsValid())
	{
		return;
	}

	if (bBridgeEndpointRootIndexDirty)
	{
		return;
	}

	EndpointIndex.Add(Root, Kind, EndpointId);
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
	if (!IsValid(Seed) || !FStructuralCircuitPromotionUtil::ComponentCarriesPower(Seed))
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

	FStructuralCircuitPromotionScope PromotionScope(this);

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
	if (!IsValid(Seed) || !FStructuralCircuitPromotionUtil::ComponentCarriesPower(Seed))
	{
		return;
	}

	FStructuralCircuitPromotionScope PromotionScope(this);

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

bool AStructuralPowerGraphSubsystem::IsPanelSupplyLinkedAndLive(
	UFGPowerConnectionComponent* InputPower,
	UFGPowerConnectionComponent* Feed) const
{
	return IsValid(InputPower)
		&& IsValid(Feed)
		&& InputPower->HasHiddenConnection(Feed)
		&& FStructuralCircuitPromotionUtil::ConnectorSuppliesPower(InputPower);
}

void AStructuralPowerGraphSubsystem::PromotePanelSupplyConnection(
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

	if (bLocalPromoteOnly || bBulkLoadDrainActive)
	{
		PromoteDirectHiddenLinks(Seed);
	}
	else
	{
		PromoteStructuralMeshFrom(Seed);
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

	UFGStructuralPowerConnectionComponent* Seed = FStructuralCircuitPromotionUtil::ComponentCarriesPower(OutletBus)
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

	bool bLinked = false;
	EndpointIndex.ForEachBridgeOnRoot(Root, [&](const FStructuralNodeId& PeerId)
	{
		if (bLinked || PeerId == SelfId)
		{
			return;
		}

		const FTrackedEndpoint* PeerTracked = TrackedEndpoints.Find(PeerId);
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
		else if (!ShouldMeshEndpoints(Host, SiblingHost, Root))
		{
			return;
		}

		if (!ShouldEndpointParticipateInRestitch(SiblingHost, PeerTracked->Kind))
		{
			return;
		}

		if (UFGStructuralPowerConnectionComponent* SiblingBus = FindBusConnector(SiblingHost))
		{
			bLinked = LinkHiddenPairLocal(OutletBus, SiblingBus);
		}
	});

	return bLinked;
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
			FStructuralPowerSwitchProcessor::RestitchKeyedConsumersOnRoot(*this, Root, SwitchControl);
		}
	}

	UE_LOG(LogStructuralPower, Log,
		TEXT("[PWR] Post-load bulk drain complete — %d bridge component root(s)"),
		Roots.Num());

	CrossSiteGraph.SeedFeedSignaturesForSites(*this, Roots);
}

UFGStructuralPowerConnectionComponent* AStructuralPowerGraphSubsystem::FindPoweredHiddenReachable(
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

	if (const FStructuralEndpointOverrides* Found = IdRegistry.FindPlayerOverride(MakeNodeId(Buildable)))
	{
		Out = *Found;
		return Out.HasAnyOverride();
	}

	return false;
}

FName AStructuralPowerGraphSubsystem::GetOrCreateComponentDefaultId(
	const FStructuralComponentKey& ComponentKey)
{
	return IdRegistry.GetOrCreateComponentDefaultId(ComponentKey);
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
		if (const FStructuralEndpointOverrides* Overrides = IdRegistry.FindPlayerOverride(MakeNodeId(Buildable));
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

	if (const FStructuralEndpointOverrides* Overrides = IdRegistry.FindPlayerOverride(MakeNodeId(Buildable));
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

	if (const FStructuralEndpointOverrides* Overrides = IdRegistry.FindPlayerOverride(MakeNodeId(Buildable));
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
	IdRegistry.ApplyPlayerOverrideEdits(NodeId, Source, Control, bClearSource, bClearControl);

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
		FStructuralPowerSwitchProcessor::Process(*this, Switch);
		const FName SwitchControl = ResolveControl(Switch, EStructuralChannel::Switch);
		FStructuralPowerSwitchProcessor::RestitchKeyedConsumersOnRoot(*this, Root, SwitchControl);
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

		if (const FStructuralEndpointOverrides* Overrides = IdRegistry.FindPlayerOverride(NodeId))
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

	IdRegistry.ForEachPlayerOverride([&](const FStructuralNodeId& NodeId, const FStructuralEndpointOverrides&)
	{
		if (const TWeakObjectPtr<AFGBuildable>* Buildable = RegisteredBuildables.Find(NodeId))
		{
			ConsiderBuildable(NodeId, Buildable->Get());
		}
	});

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
		// Panels first — downstream links must exist before light reconcile classifies
		// panel-fed consumers and before direct-attach logs report powered state.
		ReconcileAllPanelEndpoints();
		ReconcileAllLightConsumers();
	}
}

bool AStructuralPowerGraphSubsystem::IsDirectSwitchFedLight(
	int32 Root,
	const FStructuralChannelKey& LightKey)
{
	if (Root == INDEX_NONE || LightKey.Source.IsNone())
	{
		return false;
	}

	RefreshBridgeEndpointRootIndex();

	if (const TArray<FStructuralNodeId>* SwitchIds =
			EndpointIndex.Get(Root, EStructuralEndpointKind::Switch))
	{
		const TArray<FStructuralNodeId> SwitchIdsSnapshot = *SwitchIds;
		for (const FStructuralNodeId& NodeId : SwitchIdsSnapshot)
		{
			if (const FTrackedEndpoint* Tracked = TrackedEndpoints.Find(NodeId))
			{
				if (AFGBuildableCircuitSwitch* Switch =
						Cast<AFGBuildableCircuitSwitch>(Tracked->Actor.Get()))
				{
					if (ResolveControl(Switch, EStructuralChannel::Switch) == LightKey.Source)
					{
						return true;
					}
				}
			}
		}
	}

	for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : TrackedEndpoints)
	{
		if (Pair.Value.Kind != EStructuralEndpointKind::Switch)
		{
			continue;
		}

		if (StructureGraph.FindRoot(Pair.Value.ParentId) != Root)
		{
			continue;
		}

		if (AFGBuildableCircuitSwitch* Switch =
				Cast<AFGBuildableCircuitSwitch>(Pair.Value.Actor.Get()))
		{
			if (ResolveControl(Switch, EStructuralChannel::Switch) == LightKey.Source)
			{
				return true;
			}
		}
	}

	return false;
}

bool AStructuralPowerGraphSubsystem::IsPanelDownstreamLight(
	int32 Root,
	const FStructuralChannelKey& LightKey)
{
	if (Root == INDEX_NONE || LightKey.Source.IsNone() || IsDirectSwitchFedLight(Root, LightKey))
	{
		return false;
	}

	auto PanelHostsLightGroup = [&](AFGBuildableLightsControlPanel* Panel) -> bool
	{
		if (!IsValid(Panel))
		{
			return false;
		}

		const FStructuralWallAnchor Anchor = ResolveOutletAnchor(Panel);
		FStructuralNodeId ParentId;
		if (ResolveEndpointComponentRoot(Panel, Anchor, ParentId) != Root)
		{
			return false;
		}

		const FName PanelControl = ResolveControl(Panel, EStructuralChannel::Light);
		return !PanelControl.IsNone()
			&& PanelControl != StructuralPowerConstants::ControlUnconfigured
			&& PanelControl == LightKey.Source;
	};

	RefreshBridgeEndpointRootIndex();

	if (const TArray<FStructuralNodeId>* PanelIds =
			EndpointIndex.Get(Root, EStructuralEndpointKind::Panel))
	{
		const TArray<FStructuralNodeId> PanelIdsSnapshot = *PanelIds;
		for (const FStructuralNodeId& PanelId : PanelIdsSnapshot)
		{
			if (const FTrackedEndpoint* Tracked = TrackedEndpoints.Find(PanelId))
			{
				if (PanelHostsLightGroup(
						Cast<AFGBuildableLightsControlPanel>(Tracked->Actor.Get())))
				{
					return true;
				}
			}
		}
	}

	for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : TrackedEndpoints)
	{
		if (Pair.Value.Kind != EStructuralEndpointKind::Panel)
		{
			continue;
		}

		if (PanelHostsLightGroup(
				Cast<AFGBuildableLightsControlPanel>(Pair.Value.Actor.Get())))
		{
			return true;
		}
	}

	bool bFoundPendingPanel = false;
	PlacementQueue.ForEachPendingOutletBuildable([&](AFGBuildable* Buildable)
	{
		if (!bFoundPendingPanel
			&& PanelHostsLightGroup(Cast<AFGBuildableLightsControlPanel>(Buildable)))
		{
			bFoundPendingPanel = true;
		}
	});

	if (bFoundPendingPanel)
	{
		return true;
	}

	if (UWorld* World = GetWorld())
	{
		if (AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(World))
		{
			for (AFGBuildable* Buildable : BuildableSubsystem->GetAllBuildablesRef())
			{
				if (PanelHostsLightGroup(Cast<AFGBuildableLightsControlPanel>(Buildable)))
				{
					return true;
				}
			}
		}
	}

	return false;
}

bool AStructuralPowerGraphSubsystem::IsSwitchFeedOpen(
	int32 Root,
	FName SwitchControlId)
{
	if (Root == INDEX_NONE || SwitchControlId.IsNone())
	{
		return false;
	}

	if (const TArray<FStructuralNodeId>* SwitchIds =
			EndpointIndex.Get(Root, EStructuralEndpointKind::Switch))
	{
		for (const FStructuralNodeId& NodeId : *SwitchIds)
		{
			if (const FTrackedEndpoint* Tracked = TrackedEndpoints.Find(NodeId))
			{
				if (AFGBuildableCircuitSwitch* Switch =
						Cast<AFGBuildableCircuitSwitch>(Tracked->Actor.Get()))
				{
					if (ResolveControl(Switch, EStructuralChannel::Switch) != SwitchControlId)
					{
						continue;
					}

					return !FStructuralPowerSwitchProcessor::ShouldInjectStructuralPath(Switch) || !Switch->IsBridgeActive();
				}
			}
		}
	}

	return false;
}

void AStructuralPowerGraphSubsystem::LogPanelReconcileSummary(
	AFGBuildableLightsControlPanel* Panel)
{
	if (!IsValid(Panel))
	{
		return;
	}

	FStructuralPanelPorts Ports;
	if (!FStructuralPanelPortResolver::Resolve(Panel, Ports))
	{
		return;
	}

	const FStructuralChannelKey ChannelKey = ResolveChannelKeyForBuildable(Panel);
	const FStructuralWallAnchor ParentAnchor = ResolveOutletAnchor(Panel);
	FStructuralNodeId ParentId;
	const int32 Root = ResolveEndpointComponentRoot(Panel, ParentAnchor, ParentId);
	const UFGPowerConnectionComponent* InputPower =
		FStructuralPanelPortResolver::AsPowerConnection(Ports.Input);
	const int32 BusCircuit = IsValid(InputPower) ? InputPower->GetCircuitID() : INDEX_NONE;
	const int32 Powered = FStructuralCircuitPromotionUtil::ConnectorSuppliesPower(InputPower) ? 1 : 0;
	const int32 Controlled =
		Panel->GetControlledBuildables(AFGBuildableLightSource::StaticClass()).Num();

	UE_LOG(LogStructuralPower, Log,
		TEXT("[PWR] panel reconcile %s root=%d powered=%d busCircuit=%d source=%s control=%s controlled=%d"),
		*Panel->GetName(),
		Root,
		Powered,
		BusCircuit,
		*FStructuralPowerTrace::FormatSourceForTrace(ChannelKey),
		*FStructuralPowerTrace::FormatControlForTrace(ChannelKey),
		Controlled);
}

void AStructuralPowerGraphSubsystem::CollectKnownPanelEndpoints(
	TArray<AFGBuildableLightsControlPanel*>& OutPanels)
{
	OutPanels.Reset();
	TSet<AFGBuildableLightsControlPanel*> Seen;

	auto ConsiderPanel = [&](AFGBuildableLightsControlPanel* Panel)
	{
		if (!IsValid(Panel) || !Panel->HasAuthority() || Seen.Contains(Panel))
		{
			return;
		}

		Seen.Add(Panel);
		OutPanels.Add(Panel);
	};

	RefreshBridgeEndpointRootIndex();

	EndpointIndex.ForEachEndpoint(EStructuralEndpointKind::Panel, [&](const FStructuralNodeId& PanelId)
	{
		if (const FTrackedEndpoint* Tracked = TrackedEndpoints.Find(PanelId))
		{
			ConsiderPanel(Cast<AFGBuildableLightsControlPanel>(Tracked->Actor.Get()));
		}
	});

	for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : TrackedEndpoints)
	{
		if (Pair.Value.Kind != EStructuralEndpointKind::Panel)
		{
			continue;
		}

		ConsiderPanel(Cast<AFGBuildableLightsControlPanel>(Pair.Value.Actor.Get()));
	}

	PlacementQueue.ForEachPendingOutletBuildable([&](AFGBuildable* Buildable)
	{
		ConsiderPanel(Cast<AFGBuildableLightsControlPanel>(Buildable));
	});

	if (Seen.Num() > 0)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(World))
		{
			for (AFGBuildable* Buildable : BuildableSubsystem->GetAllBuildablesRef())
			{
				ConsiderPanel(Cast<AFGBuildableLightsControlPanel>(Buildable));
			}
		}
	}
}

void AStructuralPowerGraphSubsystem::ReconcileAllPanelEndpoints()
{
	if (!FStructuralPowerModConfig::IsGroupLightingEnabled())
	{
		return;
	}

	TArray<AFGBuildableLightsControlPanel*> Panels;
	CollectKnownPanelEndpoints(Panels);

	for (AFGBuildableLightsControlPanel* Panel : Panels)
	{
		const FStructuralNodeId PanelId = MakeNodeId(Panel);
		FTrackedEndpoint& Tracked = TrackedEndpoints.FindOrAdd(PanelId);
		Tracked.bPanelLinksReady = false;
		Tracked.bDownstreamLinksReady = false;
		ProcessPanelEndpoint(Panel);
		LogPanelReconcileSummary(Panel);
	}

	UE_LOG(LogStructuralPower, Log,
		TEXT("[PWR] Post-load panel reconcile complete — %d panel(s)"),
		Panels.Num());

	ApplyKeyedSubnetAllPanels();
}

void AStructuralPowerGraphSubsystem::ApplyKeyedSubnetAllPanels()
{
	if (!FStructuralPowerModConfig::IsGroupLightingEnabled())
	{
		return;
	}

	TArray<AFGBuildableLightsControlPanel*> Panels;
	CollectKnownPanelEndpoints(Panels);

	for (AFGBuildableLightsControlPanel* Panel : Panels)
	{
		FStructuralPanelControlledSync::ApplyKeyedSubnet(*this, Panel);
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
			&& FStructuralCircuitPromotionUtil::ComponentCarriesPower(Cast<UFGPowerConnectionComponent>(Cached)))
		{
			return Cached;
		}
		SourceConnectorByRoot.Remove(ComponentRoot);
	}

	UFGPowerConnectionComponent* PoweredVisible = nullptr;
	UFGStructuralPowerConnectionComponent* PoweredBus = nullptr;

	EndpointIndex.ForEachOnRoot(ComponentRoot, [&](const FStructuralNodeId& NodeId)
	{
		const FTrackedEndpoint* Tracked = TrackedEndpoints.Find(NodeId);
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
			if (UFGStructuralPowerConnectionComponent* Bus = FindBusConnector(Host))
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
		SourceConnectorByRoot.Add(ComponentRoot, PoweredVisible);
		return PoweredVisible;
	}

	if (PoweredBus)
	{
		SourceConnectorByRoot.Add(ComponentRoot, PoweredBus);
		return PoweredBus;
	}

	EndpointIndex.ForEachOnRoot(ComponentRoot, [&](const FStructuralNodeId& NodeId)
	{
		if (SourceConnectorByRoot.Contains(ComponentRoot))
		{
			return;
		}

		const FTrackedEndpoint* Tracked = TrackedEndpoints.Find(NodeId);
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

		if (UFGStructuralPowerConnectionComponent* Bus = FindBusConnector(Host))
		{
			if (UFGStructuralPowerConnectionComponent* Reachable = FindPoweredHiddenReachable(Bus))
			{
				SourceConnectorByRoot.Add(ComponentRoot, Reachable);
			}
		}
	});

	if (const TWeakObjectPtr<UFGCircuitConnectionComponent>* CachedEntry =
			SourceConnectorByRoot.Find(ComponentRoot))
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

			if (!FStructuralPowerSwitchProcessor::ShouldInjectStructuralPath(Switch))
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
		return FStructuralCircuitPromotionUtil::ComponentCarriesPower(Cast<UFGPowerConnectionComponent>(Source));
	}

	return false;
}

bool AStructuralPowerGraphSubsystem::FindNearestStructureAnchorForEquipment(
	const FVector& QueryLoc,
	float MaxHorizontal,
	float MaxVertical,
	FVector& OutAnchor,
	int32& OutComponentRoot) const
{
	return StructureGraph.FindNearestStructureAnchor(
		QueryLoc,
		MaxHorizontal,
		MaxVertical,
		OutAnchor,
		OutComponentRoot);
}

bool AStructuralPowerGraphSubsystem::QueryHoverpackStructuralAnchor(
	const FVector& QueryLoc,
	float MaxHorizontal,
	float MaxVertical,
	FStructuralHoverpackAnchorQuery& Out) const
{
	return FStructuralEquipmentBridgeRegistry::Get().QueryHoverpackStructuralAnchor(
		*const_cast<AStructuralPowerGraphSubsystem*>(this),
		QueryLoc,
		MaxHorizontal,
		MaxVertical,
		Out);
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

	return FStructuralPowerSwitchProcessor::ShouldInjectStructuralPath(Switch);
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

void AStructuralPowerGraphSubsystem::OnSwitchStateChanged(AFGBuildableCircuitSwitch* Switch)
{
	FStructuralPowerSwitchProcessor::OnStateChanged(*this, Switch);
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

void AStructuralPowerGraphSubsystem::RestitchComponent(int32 Root, bool bTearDownFirst)
{
	if (Root == INDEX_NONE)
	{
		return;
	}

	FStructuralPowerSwitchProcessor::StripInactiveLinksOnRoot(*this, Root);
	RefreshBridgeEndpointRootIndex();

	FStructuralSiteBusMesh::Remesh(
		Root,
		bTearDownFirst,
		EndpointIndex,
		[this](const FStructuralNodeId& NodeId) -> const FTrackedEndpoint*
		{
			return TrackedEndpoints.Find(NodeId);
		},
		[this](AFGBuildable* Host, EStructuralEndpointKind Kind)
		{
			return ShouldEndpointParticipateInRestitch(Host, Kind);
		},
		[this](AFGBuildable* Host)
		{
			return GetOrCreateBusConnector(Host);
		},
		[this](AFGBuildable* Host, UFGStructuralPowerConnectionComponent* Bus)
		{
			LinkBusToVisibleConnections(Host, Bus);
		},
		[this](AFGBuildable* HostA, AFGBuildable* HostB, int32 ComponentRoot)
		{
			return ShouldMeshEndpoints(HostA, HostB, ComponentRoot);
		},
		[this](UFGPowerConnectionComponent* A, UFGPowerConnectionComponent* B)
		{
			return LinkHiddenPair(A, B);
		},
		[](const UFGPowerConnectionComponent* Component)
		{
			return FStructuralCircuitPromotionUtil::ComponentCarriesPower(Component);
		},
		[this](UFGStructuralPowerConnectionComponent* StartHidden)
		{
			return FindPoweredHiddenReachable(StartHidden);
		},
		[this](UFGStructuralPowerConnectionComponent* Seed)
		{
			PromoteStructuralMeshFrom(Seed);
		});
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
		FStructuralPowerSwitchProcessor::Process(*this, Cast<AFGBuildableCircuitSwitch>(Buildable));
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
		FStructuralCircuitPromotionUtil::ComponentCarriesPower(OutletBus) ? 1 : 0);
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
			AddEndpointToRootIndex(Root, EStructuralEndpointKind::Pole, PoleId);
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
			FStructuralCircuitPromotionUtil::ComponentCarriesPower(OutletBus) ? 1 : 0,
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
			FStructuralCircuitPromotionUtil::ComponentCarriesPower(OutletBus) ? 1 : 0,
			StructuralChannelToString(EStructuralChannel::Structure),
			TEXT("-"));
	}
}

void AStructuralPowerGraphSubsystem::TearDownLightStructuralLinks(AFGBuildableLightSource* Light)
{
	FStructuralPowerLightProcessor::TearDown(*this, Light);
}

void AStructuralPowerGraphSubsystem::ProcessLightEndpoint(
	AFGBuildableLightSource* Light,
	bool bLocalPromoteOnly)
{
	FStructuralPowerLightProcessor::Process(*this, Light, bLocalPromoteOnly);
}

void AStructuralPowerGraphSubsystem::TearDownPanelStructuralLinks(
	AFGBuildableLightsControlPanel* Panel)
{
	FStructuralPowerPanelProcessor::TearDown(*this, Panel);
}

void AStructuralPowerGraphSubsystem::ProcessPanelEndpoint(
	AFGBuildableLightsControlPanel* Panel,
	bool bLocalPromoteOnly)
{
	FStructuralPowerPanelProcessor::Process(*this, Panel, bLocalPromoteOnly);
}

void AStructuralPowerGraphSubsystem::RestitchPanelEndpointsForRoot(int32 Root)
{
	FStructuralPowerPanelProcessor::RestitchOnRoot(*this, Root);
}

void AStructuralPowerGraphSubsystem::RestitchPanelsWithControlOnRoot(int32 Root, FName ControlId)
{
	FStructuralPowerPanelProcessor::RestitchWithControlOnRoot(*this, Root, ControlId);
}

void AStructuralPowerGraphSubsystem::RestitchLightEndpointsForRoot(int32 Root)
{
	FStructuralPowerLightProcessor::RestitchOnRoot(*this, Root);
}

void AStructuralPowerGraphSubsystem::EnumerateTrackedLightsOnRoot(
	int32 Root,
	TFunctionRef<void(AFGBuildableLightSource*)> Visitor)
{
	if (Root == INDEX_NONE)
	{
		return;
	}

	RefreshBridgeEndpointRootIndex();

	const TArray<FStructuralNodeId>* LightIds =
		EndpointIndex.Get(Root, EStructuralEndpointKind::Light);
	if (!LightIds)
	{
		return;
	}

	for (const FStructuralNodeId& LightId : *LightIds)
	{
		const FTrackedEndpoint* Tracked = TrackedEndpoints.Find(LightId);
		if (!Tracked)
		{
			continue;
		}

		if (AFGBuildableLightSource* Light = Cast<AFGBuildableLightSource>(Tracked->Actor.Get()))
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

	RefreshBridgeEndpointRootIndex();
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
				FStructuralPowerSwitchProcessor::TearDown(*this, Host);
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

	PlacementQueue.RemoveBuildable(Buildable);
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

	PlacementQueue.RemoveLightweight(Key);
}

void AStructuralPowerGraphSubsystem::RegisterBuildableActor(AFGBuildable* Buildable)
{
	if (!IsValid(Buildable))
	{
		return;
	}

	RegisteredBuildables.Add(MakeNodeId(Buildable), Buildable);
	if (FStructuralEligibilityRules::IsBusMember(Buildable))
	{
		BusMemberSpatialIndex.RegisterMember(Buildable);
	}
}

void AStructuralPowerGraphSubsystem::UnregisterBuildableActor(const FStructuralNodeId& NodeId)
{
	if (!NodeId.IsValid() || NodeId.IsLightweight())
	{
		return;
	}

	if (const TWeakObjectPtr<AFGBuildable>* Entry = RegisteredBuildables.Find(NodeId))
	{
		if (AFGBuildable* Buildable = Entry->Get())
		{
			BusMemberSpatialIndex.UnregisterMember(Buildable);
		}
	}

	RegisteredBuildables.Remove(NodeId);
}

void AStructuralPowerGraphSubsystem::RebuildBuildableRegistry(UWorld* World)
{
	RegisteredBuildables.Reset();
	BusMemberSpatialIndex.Reset();

	if (!IsValid(World))
	{
		return;
	}

	BusMemberSpatialIndex.RebuildFromWorld(World);

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
