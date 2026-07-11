// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Save/AStructuralPowerGraphSubsystem.h"

#include "Attach/FStructuralDeviceAttach.h"
#include "Attach/FStructuralPanelAttach.h"
#include "Connection/FStructuralPoleConnectionPoint.h"
#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableGenerator.h"
#include "Buildables/FGBuildableLightSource.h"
#include "Buildables/FGBuildableLightsControlPanel.h"
#include "Buildables/FGBuildableCircuitBridge.h"
#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "Buildables/FGBuildablePowerStorage.h"
#include "Circuit/FStructuralCircuitPromotionScope.h"
#include "Circuit/FStructuralCircuitPromotionUtil.h"
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
#include "Graph/FStructuralPowerBuildableCasts.h"
#include "Graph/FStructuralAttachmentResolver.h"
#include "Graph/FStructuralOutletParentResolver.h"
#include "Graph/FStructuralEndpointIndex.h"
#include "Graph/FStructuralBusMemberSpatialIndex.h"
#include "Routing/FStructuralMembershipRole.h"
#include "Routing/FStructuralPowerRouter.h"
#include "Kismet/GameplayStatics.h"
#include "Lightweight/FStructuralLightweightIndex.h"
#include "Graph/FStructuralSwitchParentResolver.h"
#include "Network/UStructuralPowerSwitchListener.h"
#include "Network/UStructuralPowerPanelListener.h"
#include "Network/UStructuralPowerStorageListener.h"
#include "Panel/FStructuralPanelControlledSync.h"
#include "Panel/FStructuralPanelPortResolver.h"
#include "Processors/FStructuralPowerGeneratorProcessor.h"
#include "Processors/FStructuralPowerLightProcessor.h"
#include "Processors/FStructuralPowerPanelProcessor.h"
#include "Processors/FStructuralPowerProcessorRegistry.h"
#include "Processors/FStructuralPowerPoleProcessor.h"
#include "Processors/FStructuralPowerSwitchProcessor.h"
#include "Processors/FStructuralPowerTransferGate.h"
#include "Processors/IStructuralPowerProcessor.h"
#include "Rules/FStructuralEligibilityRules.h"
#include "Session/FStructuralPowerSessionSettings.h"
#include "StructuralPowerConstants.h"
#include "StructuralPowerLog.h"
#include "TimerManager.h"
#include "HAL/PlatformTime.h"

AStructuralPowerGraphSubsystem::AStructuralPowerGraphSubsystem()
{
	bReplicates = false;
	IdRegistry.Bind(ComponentDefaultIds, PlayerEndpointOverrides);
	ReconcileOps.Bind(this);
	RestitchOps.Bind(this);
	CircuitOps.Bind(this);
	BridgeRootIndex.Bind(this);
	IdOps.Bind(this);
	WireDeltaHandler.Bind(this);
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

UFGStructuralPowerConnectionComponent*
AStructuralPowerGraphSubsystem::FindBusConnector(const AFGBuildable* Host)
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
			&& Connector->GetFName() == StructuralPowerConstants::OutletBusConnectorName)
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

UFGStructuralPowerConnectionComponent* AStructuralPowerGraphSubsystem::FindSwitchControlBus(
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
			&& Connector->GetFName() == StructuralPowerConstants::SwitchControlBusConnectorName)
		{
			return Connector;
		}
	}

	return nullptr;
}

void AStructuralPowerGraphSubsystem::StripPersistedEndpointModComponents(AFGBuildable* Host)
{
	// Saved outlet/panel buses still reference torn mesh — strip before live restitch or load asserts.
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
			&& BusName != StructuralPowerConstants::PanelControlBusConnectorName
			&& BusName != StructuralPowerConstants::SwitchControlBusConnectorName)
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

	if (Host->IsA<AFGBuildablePowerStorage>())
	{
		TInlineComponentArray<UStructuralPowerStorageListener*> Listeners;
		Host->GetComponents(Listeners);
		for (UStructuralPowerStorageListener* Listener : Listeners)
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

UFGStructuralPowerConnectionComponent*
AStructuralPowerGraphSubsystem::FindOutletBusConnector(const AFGBuildablePowerPole* Outlet)
{
	return FindBusConnector(Outlet);
}

UFGStructuralPowerConnectionComponent*
AStructuralPowerGraphSubsystem::GetOrCreateBusConnector(AFGBuildable* Host)
{
	if (!IsValid(Host))
	{
		return nullptr;
	}

	const bool bPole = Host->IsA<AFGBuildablePowerPole>();
	const bool bSwitch = Host->IsA<AFGBuildableCircuitSwitch>();
	const bool bStorage = Host->IsA<AFGBuildablePowerStorage>();
	if (!bPole && !bSwitch && !bStorage)
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

UFGStructuralPowerConnectionComponent* AStructuralPowerGraphSubsystem::GetOrCreateSwitchControlBus(
	AFGBuildableCircuitSwitch* Switch)
{
	if (!IsValid(Switch))
	{
		return nullptr;
	}

	if (UFGStructuralPowerConnectionComponent* Existing = FindSwitchControlBus(Switch))
	{
		return Existing;
	}

	UFGStructuralPowerConnectionComponent* Connector = NewObject<UFGStructuralPowerConnectionComponent>(
		Switch,
		StructuralPowerConstants::SwitchControlBusConnectorName,
		RF_Transient);
	if (!Connector)
	{
		return nullptr;
	}

	Connector->SetMobility(EComponentMobility::Static);
	Switch->AddInstanceComponent(Connector);
	Connector->RegisterComponent();
	return Connector;
}

UFGStructuralPowerConnectionComponent*
AStructuralPowerGraphSubsystem::GetOrCreateOutletBusConnector(AFGBuildablePowerPole* Outlet)
{
	return GetOrCreateBusConnector(Outlet);
}

FStructuralWallAnchor
AStructuralPowerGraphSubsystem::ResolveOutletAnchor(AFGBuildable* Outlet) const
{
	return FStructuralAttachmentResolver::ResolveStructuralParent(
		Outlet,
		GetWorld(),
		MakeOutletParentResolveParams());
}

FStructuralOutletParentResolveParams
AStructuralPowerGraphSubsystem::MakeOutletParentResolveParams() const
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

FStructuralNodeId
AStructuralPowerGraphSubsystem::MakeParentNodeId(const FStructuralWallAnchor& Anchor)
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
	SiteState.ClearFeedSignatures();

	RebuildBuildableRegistry(World);
	RebuildLightweightIndex(World);
	StructureGraph.Rebuild(World);
	PurgeSavedOutletBusMesh(World);
	bPostLoadRebuilt = true;

	int32 SeededPoles = 0;
	int32 SeededSwitches = 0;
	int32 SeededStorage = 0;
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
			else if (FStructuralEligibilityRules::IsPowerBridgeSwitch(Buildable))
			{
				EnqueuePlacement(Buildable, EStructuralPlacementJobType::Outlet, /*bDefer=*/true);
				++SeededSwitches;
			}
			else if (FStructuralEligibilityRules::IsPowerStorage(Buildable))
			{
				EnqueuePlacement(Buildable, EStructuralPlacementJobType::Outlet, /*bDefer=*/true);
				++SeededStorage;
			}
		}
	}

	int32 Components = 0;
	int32 Largest = 0;
	StructureGraph.GetComponentStats(Components, Largest);

	UE_LOG(LogStructuralPower, Log,
		TEXT("Graph ready — %d structure node(s), %d component(s) (largest %d), %d LW tracked,"
			" %d pole(s) seeded, %d switch(es) seeded, %d storage seeded"),
		StructureGraph.GetNodeCount(),
		Components,
		Largest,
		LightweightIndex.GetTrackedCount(),
		SeededPoles,
		SeededSwitches,
		SeededStorage);

	// Per-pole Restitch during save load was multi-second — drain in tick budget instead.
	bBulkLoadDrainActive = (SeededPoles + SeededSwitches + SeededStorage) > 0;
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
		const bool bSwitch = FStructuralEligibilityRules::IsPowerBridgeSwitch(Buildable);
		const bool bStorage = FStructuralEligibilityRules::IsPowerStorage(Buildable);
		const bool bPanel = FStructuralPowerModConfig::IsGroupLightingEnabled()
			&& Buildable->IsA<AFGBuildableLightsControlPanel>();
		if (!bPole && !bSwitch && !bStorage && !bPanel)
		{
			continue;
		}

		const bool bHasModBus = IsValid(FindBusConnector(Buildable))
			|| (bSwitch && IsValid(FindSwitchControlBus(Buildable)))
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
	if (!Key.IsValid())
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
	CircuitOps.BeginCircuitPromotion();
}


void AStructuralPowerGraphSubsystem::EndCircuitPromotion()
{
	CircuitOps.EndCircuitPromotion();
}


bool AStructuralPowerGraphSubsystem::LinkHiddenPair(
	UFGPowerConnectionComponent* A,
	UFGPowerConnectionComponent* B,
	bool bPromoteCircuit)
{
	return CircuitOps.LinkHiddenPair(A, B, bPromoteCircuit);
}


bool AStructuralPowerGraphSubsystem::LinkHiddenPairLocal(
	UFGPowerConnectionComponent* A,
	UFGPowerConnectionComponent* B,
	bool bPromoteCircuit)
{
	return CircuitOps.LinkHiddenPairLocal(A, B, bPromoteCircuit);
}


void AStructuralPowerGraphSubsystem::MarkBridgeEndpointRootIndexDirty()
{
	BridgeRootIndex.MarkBridgeEndpointRootIndexDirty();
}


void AStructuralPowerGraphSubsystem::RefreshBridgeEndpointRootIndex()
{
	BridgeRootIndex.RefreshBridgeEndpointRootIndex();
}


void AStructuralPowerGraphSubsystem::AddEndpointToRootIndex(
	int32 Root,
	EStructuralEndpointKind Kind,
	const FStructuralNodeId& EndpointId)
{
	BridgeRootIndex.AddEndpointToRootIndex(Root, Kind, EndpointId);
}


int32 AStructuralPowerGraphSubsystem::FindRootForTrackedEndpoint(
	const FTrackedEndpoint& Tracked) const
{
	return BridgeRootIndex.FindRootForTrackedEndpoint(Tracked);
}


int32 AStructuralPowerGraphSubsystem::ResolveBridgeRootFromAnchor(
	AFGBuildable* Host,
	const FStructuralWallAnchor& Anchor,
	FStructuralNodeId& OutParentId,
	bool bPreferBulkResolve)
{
	return BridgeRootIndex.ResolveBridgeRootFromAnchor(Host, Anchor, OutParentId, bPreferBulkResolve);
}


void AStructuralPowerGraphSubsystem::PromoteStructuralMeshFrom(UFGPowerConnectionComponent* Seed)
{
	CircuitOps.PromoteStructuralMeshFrom(Seed);
}


void AStructuralPowerGraphSubsystem::PromoteDirectHiddenLinks(UFGPowerConnectionComponent* Seed)
{
	CircuitOps.PromoteDirectHiddenLinks(Seed);
}


bool AStructuralPowerGraphSubsystem::IsPanelSupplyLinked(
	UFGPowerConnectionComponent* InputPower,
	UFGPowerConnectionComponent* Feed) const
{
	return CircuitOps.IsPanelSupplyLinked(InputPower, Feed);
}

bool AStructuralPowerGraphSubsystem::IsPanelSupplyLinkedAndLive(
	UFGPowerConnectionComponent* InputPower,
	UFGPowerConnectionComponent* Feed) const
{
	return CircuitOps.IsPanelSupplyLinkedAndLive(InputPower, Feed);
}


void AStructuralPowerGraphSubsystem::PromotePanelSupplyConnection(
	UFGPowerConnectionComponent* InputPower,
	UFGPowerConnectionComponent* Feed,
	bool bLocalPromoteOnly)
{
	CircuitOps.PromotePanelSupplyConnection(InputPower, Feed, bLocalPromoteOnly);
}


void AStructuralPowerGraphSubsystem::PromoteOutletBusIfPowered(
	UFGStructuralPowerConnectionComponent* OutletBus,
	bool bLocalPromoteOnly)
{
	CircuitOps.PromoteOutletBusIfPowered(OutletBus, bLocalPromoteOnly);
}


void AStructuralPowerGraphSubsystem::ApplyLocalBridgeBusAttach(
	AFGBuildable* Host,
	UFGStructuralPowerConnectionComponent* OutletBus,
	int32 Root,
	const FStructuralNodeId& SelfId,
	const AFGBuildable* FeedExcludeHost)
{
	CircuitOps.ApplyLocalBridgeBusAttach(Host, OutletBus, Root, SelfId, FeedExcludeHost);
}


bool AStructuralPowerGraphSubsystem::TryMeshPeerBusOnComponent(
	AFGBuildable* Host,
	UFGStructuralPowerConnectionComponent* OutletBus,
	int32 Root,
	const FStructuralNodeId& SelfId,
	bool bBridgePeersOnly,
	bool bMeshOnlyLinks)
{
	return CircuitOps.TryMeshPeerBusOnComponent(
		Host,
		OutletBus,
		Root,
		SelfId,
		bBridgePeersOnly,
		bMeshOnlyLinks);
}


int32 AStructuralPowerGraphSubsystem::ResolveBridgeComponentRootBulk(
	AFGBuildable* Host,
	const FStructuralWallAnchor& Anchor,
	FStructuralNodeId& OutParentId)
{
	return BridgeRootIndex.ResolveBridgeComponentRootBulk(Host, Anchor, OutParentId);
}


int32 AStructuralPowerGraphSubsystem::ResolvePoleComponentRoot(
	AFGBuildablePowerPole* Pole,
	const FStructuralWallAnchor& Anchor,
	FStructuralNodeId& OutParentId)
{
	return BridgeRootIndex.ResolvePoleComponentRoot(Pole, Anchor, OutParentId);
}


EAttachContext AStructuralPowerGraphSubsystem::GetCurrentAttachContext() const
{
	return AttachContextFromBulkDrain(bBulkLoadDrainActive);
}

FStructuralPowerContext AStructuralPowerGraphSubsystem::MakeProcessorContext(
	const EAttachContext AttachContext,
	const int32 SiteRoot) const
{
	return FStructuralPowerContext(
		const_cast<AStructuralPowerGraphSubsystem&>(*this),
		AttachContext,
		SiteRoot);
}

FStructuralPowerContext AStructuralPowerGraphSubsystem::GetProcessorContext() const
{
	return MakeProcessorContext(GetCurrentAttachContext());
}

bool AStructuralPowerGraphSubsystem::ShouldDeferCircuitDrivenRefresh() const
{
	return bBulkLoadDrainActive
		|| bPendingPostLoadLightReconcile
		|| bPendingFinalLightingReconcile
		|| CircuitPromotionDepth > 0;
}

void AStructuralPowerGraphSubsystem::FinishBulkLoadDrain()
{
	if (!bBulkLoadDrainActive)
	{
		return;
	}

	MarkBridgeEndpointRootIndexDirty();
	RefreshBridgeEndpointRootIndex();

	FStructuralPowerContext SwitchCtx = MakeProcessorContext(EAttachContext::BulkLoad);
	FStructuralPowerSwitchProcessor::RemeshUnmeshedPeersAfterBulkLoad(SwitchCtx);

	TSet<int32> Roots;
	for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : TrackedEndpoints)
	{
		if (Pair.Value.Kind != EStructuralEndpointKind::Pole
			&& Pair.Value.Kind != EStructuralEndpointKind::Switch
			&& Pair.Value.Kind != EStructuralEndpointKind::Storage)
		{
			continue;
		}

		if (Pair.Value.Kind == EStructuralEndpointKind::Pole
			&& !Pair.Value.bStructuralPowerTransferActive)
		{
			continue;
		}

		const int32 Root = StructureGraph.FindRoot(Pair.Value.ParentId);
		if (Root != INDEX_NONE)
		{
			Roots.Add(Root);
		}
	}

	if (Roots.Num() > 0)
	{
		const TArray<int32> RootArray = Roots.Array();

		FStructuralPowerContext BulkCtx = MakeProcessorContext(EAttachContext::BulkLoad);
		for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : TrackedEndpoints)
		{
			if (Pair.Value.Kind != EStructuralEndpointKind::Switch)
			{
				continue;
			}

			if (AFGBuildableCircuitSwitch* Switch = Pair.Value.GetSwitch())
			{
				FStructuralPowerSwitchProcessor::ApplySwitchBridgeStrategy(BulkCtx, Switch);
			}
		}

		for (int32 Root : RootArray)
		{
			FStructuralPowerContext RootCtx = MakeProcessorContext(EAttachContext::BulkLoad, Root);
			FStructuralPowerTransferGate::RefreshKeyedTransferOnRoot(
				RootCtx,
				Root,
				/*bLocalPromoteOnly=*/false);
		}

		for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : TrackedEndpoints)
		{
			if (Pair.Value.Kind != EStructuralEndpointKind::Switch)
			{
				continue;
			}

			if (AFGBuildableCircuitSwitch* Switch = Pair.Value.GetSwitch())
			{
				FStructuralPowerSwitchProcessor::EnsureListener(BulkCtx, Switch);
			}
		}
	}

	UE_LOG(LogStructuralPower, Log,
		TEXT("[HALSP] Post-load bulk drain complete — %d bridge component root(s)"),
		Roots.Num());

	CrossSiteGraph.SeedFeedSignaturesForSites(*this, Roots);
}

UFGStructuralPowerConnectionComponent* AStructuralPowerGraphSubsystem::FindPoweredHiddenReachable(
	UFGStructuralPowerConnectionComponent* StartHidden,
	int32 MaxHiddenHops) const
{
	return CircuitOps.FindPoweredHiddenReachable(StartHidden, MaxHiddenHops);
}


FStructuralComponentKey
AStructuralPowerGraphSubsystem::MakeComponentKeyForRoot(int32 ComponentRoot) const
{
	return IdOps.MakeComponentKeyForRoot(ComponentRoot);
}


FStructuralComponentKey AStructuralPowerGraphSubsystem::MakeComponentKeyForBuildable(
	const AFGBuildable* Buildable) const
{
	return IdOps.MakeComponentKeyForBuildable(Buildable);
}


bool AStructuralPowerGraphSubsystem::GetEndpointOverrides(
	const AFGBuildable* Buildable,
	FStructuralEndpointOverrides& Out) const
{
	return IdOps.GetEndpointOverrides(Buildable, Out);
}


FName AStructuralPowerGraphSubsystem::GetOrCreateComponentDefaultId(
	const FStructuralComponentKey& ComponentKey)
{
	return IdOps.GetOrCreateComponentDefaultId(ComponentKey);
}


FName AStructuralPowerGraphSubsystem::ResolveSource(
	AFGBuildable* Buildable,
	EStructuralChannel Tag)
{
	return IdOps.ResolveSource(Buildable, Tag);
}


FName AStructuralPowerGraphSubsystem::ResolveControl(
	AFGBuildable* Buildable,
	EStructuralChannel Tag)
{
	return IdOps.ResolveControl(Buildable, Tag);
}


FStructuralChannelKey AStructuralPowerGraphSubsystem::ResolveChannelKeyForBuildable(
	AFGBuildable* Buildable)
{
	return IdOps.ResolveChannelKeyForBuildable(Buildable);
}


int32 AStructuralPowerGraphSubsystem::GetEndpointComponentRoot(AFGBuildable* Endpoint)
{
	return BridgeRootIndex.GetEndpointComponentRoot(Endpoint);
}


void AStructuralPowerGraphSubsystem::SetEndpointIds(
	AFGBuildable* Buildable,
	FName Source,
	FName Control,
	bool bClearSource,
	bool bClearControl)
{
	IdOps.SetEndpointIds(Buildable, Source, Control, bClearSource, bClearControl);
}


bool AStructuralPowerGraphSubsystem::CollectIdsOnComponent(
	const FStructuralComponentKey& Key,
	FStructuralComponentIdList& Out) const
{
	return IdOps.CollectIdsOnComponent(Key, Out);
}

void AStructuralPowerGraphSubsystem::RebuildControlIdGangsForRoot(const int32 ComponentRoot)
{
	if (ComponentRoot == INDEX_NONE)
	{
		return;
	}

	const FStructuralComponentKey ComponentKey = MakeComponentKeyForRoot(ComponentRoot);
	ControlIdGangIndex.ClearComponent(ComponentKey);

	auto RegisterPublisher = [&](const FStructuralNodeId& NodeId, AFGBuildable* Buildable)
	{
		if (!IsValid(Buildable))
		{
			return;
		}

		if (MakeComponentKeyForBuildable(Buildable) != ComponentKey)
		{
			return;
		}

		const EStructuralChannel Tag = FStructuralEligibilityRules::ClassifyBuildable(Buildable);
		const EStructuralMembershipRole Role = FStructuralMembershipRole::Resolve(Buildable, Tag);
		if (!FStructuralMembershipRole::PublishesControl(Role))
		{
			return;
		}

		const FName ControlId = ResolveControl(Buildable, Tag);
		if (!FStructuralPowerRouter::IsAssignedControl(ControlId))
		{
			return;
		}

		ControlIdGangIndex.RegisterPublisher(
			NodeId,
			FStructuralControlGangKey{ComponentKey, ControlId});
	};

	for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : TrackedEndpoints)
	{
		RegisterPublisher(Pair.Key, Pair.Value.Actor.Get());
	}

	for (const TPair<FStructuralNodeId, TWeakObjectPtr<AFGBuildable>>& Pair : RegisteredBuildables)
	{
		RegisterPublisher(Pair.Key, Pair.Value.Get());
	}
}

TArray<FStructuralNodeId> AStructuralPowerGraphSubsystem::GetControlIdGangMembers(
	const FStructuralComponentKey& ComponentKey,
	const FName ControlId) const
{
	return ControlIdGangIndex.GetGangMembers(ComponentKey, ControlId);
}


int32 AStructuralPowerGraphSubsystem::ResolveEndpointComponentRoot(
	AFGBuildable* Endpoint,
	const FStructuralWallAnchor& Anchor,
	FStructuralNodeId& OutParentId)
{
	return BridgeRootIndex.ResolveEndpointComponentRoot(Endpoint, Anchor, OutParentId);
}


int32 AStructuralPowerGraphSubsystem::ResolveBridgeHostComponentRoot(
	AFGBuildable* Host,
	FStructuralNodeId* OutParentId)
{
	return BridgeRootIndex.ResolveBridgeHostComponentRoot(Host, OutParentId);
}


void AStructuralPowerGraphSubsystem::MaybeRunPostLoadLightReconcile()
{
	ReconcileOps.MaybeRunPostLoadLightReconcile();
}

void AStructuralPowerGraphSubsystem::MaybeRunFinalLightingReconcile()
{
	ReconcileOps.MaybeRunFinalLightingReconcile();
}

void AStructuralPowerGraphSubsystem::ScheduleFinalLightingReconcile()
{
	if (!FStructuralPowerModConfig::IsGroupLightingEnabled())
	{
		bPendingFinalLightingReconcile = false;
		return;
	}

	bPendingFinalLightingReconcile = true;

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	World->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			MaybeRunFinalLightingReconcile();
		}));
}


bool AStructuralPowerGraphSubsystem::IsDirectSwitchFedLight(
	int32 Root,
	const FStructuralChannelKey& LightKey)
{
	return ReconcileOps.IsDirectSwitchFedLight(Root, LightKey);
}


bool AStructuralPowerGraphSubsystem::IsPanelDownstreamLight(
	int32 Root,
	const FStructuralChannelKey& LightKey)
{
	return ReconcileOps.IsPanelDownstreamLight(Root, LightKey);
}


bool AStructuralPowerGraphSubsystem::IsSwitchFeedOpen(
	int32 Root,
	FName SwitchControlId)
{
	return ReconcileOps.IsSwitchFeedOpen(Root, SwitchControlId);
}


void AStructuralPowerGraphSubsystem::LogPanelReconcileSummary(
	AFGBuildableLightsControlPanel* Panel)
{
	ReconcileOps.LogPanelReconcileSummary(Panel);
}


void AStructuralPowerGraphSubsystem::CollectKnownPanelEndpoints(
	TArray<AFGBuildableLightsControlPanel*>& OutPanels)
{
	ReconcileOps.CollectKnownPanelEndpoints(OutPanels);
}


void AStructuralPowerGraphSubsystem::ReconcileAllPanelEndpoints()
{
	ReconcileOps.ReconcileAllPanelEndpoints();
}


void AStructuralPowerGraphSubsystem::ApplyKeyedSubnetAllPanels()
{
	ReconcileOps.ApplyKeyedSubnetAllPanels();
}


bool AStructuralPowerGraphSubsystem::EnsureParentRegisteredInGraph(
	const FStructuralWallAnchor& Anchor,
	FStructuralNodeId& OutParentId)
{
	return BridgeRootIndex.EnsureParentRegisteredInGraph(Anchor, OutParentId);
}


void AStructuralPowerGraphSubsystem::LinkBusToVisibleConnectionsLocal(
	AFGBuildable* Host,
	UFGStructuralPowerConnectionComponent* Bus,
	bool bMeshOnlyLinks)
{
	CircuitOps.LinkBusToVisibleConnectionsLocal(Host, Bus, bMeshOnlyLinks);
}


void AStructuralPowerGraphSubsystem::LinkBusToVisibleConnections(
	AFGBuildable* Host,
	UFGStructuralPowerConnectionComponent* Bus)
{
	CircuitOps.LinkBusToVisibleConnections(Host, Bus);
}


bool AStructuralPowerGraphSubsystem::HasBridgeBusPeerMesh(
	UFGStructuralPowerConnectionComponent* Bus) const
{
	return CircuitOps.HasBridgeBusPeerMesh(Bus);
}


UFGCircuitConnectionComponent* AStructuralPowerGraphSubsystem::GetComponentSourceConnector(
	int32 ComponentRoot,
	const AFGBuildable* ExcludeHost)
{
	return CircuitOps.GetComponentSourceConnector(ComponentRoot, ExcludeHost);
}


UFGPowerConnectionComponent* AStructuralPowerGraphSubsystem::ResolveSubnetFeedConnector(
	int32 ComponentRoot,
	const FStructuralChannelKey& DeviceKey)
{
	return CircuitOps.ResolveSubnetFeedConnector(ComponentRoot, DeviceKey);
}


bool AStructuralPowerGraphSubsystem::DoesComponentRootCarryPower(int32 ComponentRoot) const
{
	if (ComponentRoot == INDEX_NONE)
	{
		return false;
	}

	if (UFGCircuitConnectionComponent* Source = const_cast<AStructuralPowerGraphSubsystem*>(this)
			->GetComponentSourceConnector(ComponentRoot, nullptr))
	{
		return FStructuralCircuitPromotionUtil::ConnectorSuppliesPower(
			Cast<UFGPowerConnectionComponent>(Source));
	}

	return false;
}

bool AStructuralPowerGraphSubsystem::DoesSiteStructuralBusCarryPower(int32 ComponentRoot) const
{
	if (ComponentRoot == INDEX_NONE)
	{
		return false;
	}

	bool bPowered = false;
	EndpointIndex.ForEachBridgeOnRoot(
		ComponentRoot,
		[this, &bPowered](const FStructuralNodeId& EndpointId)
		{
			if (bPowered)
			{
				return;
			}

			const FTrackedEndpoint* Tracked = TrackedEndpoints.Find(EndpointId);
			if (!Tracked)
			{
				return;
			}

			const AFGBuildable* Host = Tracked->Actor.Get();
			if (!IsValid(Host))
			{
				return;
			}

			if (UFGStructuralPowerConnectionComponent* Bus = FindBusConnector(Host))
			{
				if (FStructuralCircuitPromotionUtil::ComponentCarriesPower(Bus))
				{
					bPowered = true;
				}
			}
		});

	return bPowered;
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
	return RestitchOps.ShouldEndpointParticipateInRestitch(Host, Kind);
}


bool AStructuralPowerGraphSubsystem::ShouldMeshEndpoints(
	AFGBuildable* HostA,
	AFGBuildable* HostB,
	int32 ComponentRoot) const
{
	return RestitchOps.ShouldMeshEndpoints(HostA, HostB, ComponentRoot);
}


void AStructuralPowerGraphSubsystem::OnSwitchStateChanged(AFGBuildableCircuitSwitch* Switch)
{
	if (ShouldDeferCircuitDrivenRefresh())
	{
		return;
	}

	FStructuralPowerContext Ctx = MakeProcessorContext(EAttachContext::Toggle);
	FStructuralPowerSwitchProcessor::OnStateChanged(Ctx, Switch);
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

	// Root fusion updates structure graph only — endpoints keep existing hidden links.
	if (MergedRoots.Num() >= 2)
	{
		const int32 Root = StructureGraph.FindRoot(MakeNodeId(Buildable));

		UE_LOG(LogStructuralPower, Verbose,
			TEXT("[HALSP] structure %s fused %d component(s) -> root %d"),
			*Buildable->GetName(),
			MergedRoots.Num(),
			Root);
	}
}

void AStructuralPowerGraphSubsystem::ProcessLightweightStructure(
	const FStructuralLightweightKey& Key)
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
		(void)StructureGraph.FindRoot(FStructuralLightweightIndex::MakeNodeId(Key));
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
		if (AFGBuildableLightSource* Light = FStructuralPowerBuildableCasts::AsLight(Buildable))
		{
			ProcessLightEndpoint(Light);
		}
		return;
	}

	if (AFGBuildableGenerator* Generator = Cast<AFGBuildableGenerator>(Buildable))
	{
		ProcessGeneratorEndpoint(Generator);
		return;
	}

	if (AFGBuildableLightsControlPanel* Panel = FStructuralPowerBuildableCasts::AsPanel(Buildable))
	{
		if (bBulkLoadDrainActive)
		{
			return;
		}

		ProcessPanelEndpoint(Panel);
		return;
	}

	if (FStructuralEligibilityRules::IsPowerBridgeSwitch(Buildable))
	{
		if (AFGBuildableCircuitSwitch* Switch = FStructuralPowerBuildableCasts::AsSwitch(Buildable))
		{
			ProcessSwitchEndpoint(Switch);
		}
		return;
	}

	if (FStructuralEligibilityRules::IsPowerBridgePole(Buildable))
	{
		if (AFGBuildablePowerPole* Pole = FStructuralPowerBuildableCasts::AsPole(Buildable))
		{
			ProcessPoleEndpoint(Pole);
		}
		return;
	}

	if (FStructuralEligibilityRules::IsPowerStorage(Buildable))
	{
		if (AFGBuildablePowerStorage* Storage = FStructuralPowerBuildableCasts::AsStorage(Buildable))
		{
			ProcessStorageEndpoint(Storage);
		}
		return;
	}

	FStructuralPowerTrace::LogPlacementSkip(Buildable, TEXT("not_bridge_endpoint"));
}

void AStructuralPowerGraphSubsystem::ProcessSwitchEndpoint(AFGBuildableCircuitSwitch* Switch)
{
	if (!IsValid(Switch))
	{
		return;
	}

	FStructuralPowerContext Ctx = GetProcessorContext();
	if (IStructuralPowerProcessor* Processor =
			FStructuralPowerProcessorRegistry::Get().FindMutable(EStructuralEndpointKind::Switch))
	{
		Processor->Process(Ctx, Switch);
	}
}

void AStructuralPowerGraphSubsystem::ProcessSwitchWireDelta(AFGBuildableCircuitSwitch* Switch)
{
	ProcessSwitchCircuitsRebuilt(Switch);
}

void AStructuralPowerGraphSubsystem::ProcessSwitchCircuitsRebuilt(AFGBuildableCircuitSwitch* Switch)
{
	if (ShouldDeferCircuitDrivenRefresh())
	{
		return;
	}

	WireDeltaHandler.ProcessSwitchCircuitsRebuilt(Switch);
}


void AStructuralPowerGraphSubsystem::ProcessPanelWireDelta(AFGBuildableLightsControlPanel* Panel)
{
	if (ShouldDeferCircuitDrivenRefresh())
	{
		return;
	}

	WireDeltaHandler.ProcessPanelWireDelta(Panel);
}


bool AStructuralPowerGraphSubsystem::ShouldSkipPanelCircuitEcho(
	AFGBuildableLightsControlPanel* Panel,
	const TCHAR** OutReason)
{
	auto SetReason = [OutReason](const TCHAR* Reason)
	{
		if (OutReason)
		{
			*OutReason = Reason;
		}
	};

	if (!IsValid(Panel) || !FStructuralPowerModConfig::IsGroupLightingEnabled())
	{
		return false;
	}

	const FStructuralChannelKey ChannelKey = IdOps.ResolveChannelKeyForBuildable(Panel);
	const FStructuralWallAnchor ParentAnchor = ResolveOutletAnchor(Panel);
	FStructuralNodeId ParentId;
	const int32 Root =
		BridgeRootIndex.ResolveEndpointComponentRoot(Panel, ParentAnchor, ParentId);

	if (!ChannelKey.Source.IsNone()
		&& Root != INDEX_NONE
		&& ReconcileOps.IsSwitchFeedOpen(Root, ChannelKey.Source))
	{
		SetReason(TEXT("skip_feed_open"));
		return true;
	}

	const FStructuralNodeId PanelId = MakeNodeId(Panel);

	if (SiteState.IsEchoScopeActive())
	{
		if (!SiteState.IsEchoDirtySite(Root))
		{
			SetReason(TEXT("skip_echo_scope"));
			return true;
		}

		if (SiteState.WasPanelToggleHandledInScope(PanelId))
		{
			SetReason(TEXT("skip_echo_toggle"));
			return true;
		}

		if (SiteState.WasPanelEchoProcessedInScope(PanelId))
		{
			SetReason(TEXT("skip_echo_coalesce"));
			return true;
		}
	}
	const FTrackedEndpoint* Tracked = TrackedEndpoints.Find(PanelId);
	if (!Tracked)
	{
		return false;
	}

	const bool bRoutingUnchanged = Tracked->bPanelLinksReady
		&& Tracked->CachedPanelRoot == Root
		&& Tracked->CachedPanelKey == ChannelKey;
	if (bRoutingUnchanged)
	{
		FStructuralPanelPorts Ports;
		if (FStructuralPanelPortResolver::Resolve(Panel, Ports))
		{
			UFGPowerConnectionComponent* InputPower =
				FStructuralPanelPortResolver::AsPowerConnection(Ports.Input);
			if (FStructuralCircuitPromotionUtil::ConnectorSuppliesPower(InputPower)
				|| FStructuralPanelAttach::SupplyAlreadyLinked(
					*this,
					Panel,
					Ports,
					Root,
					ChannelKey))
			{
				SetReason(TEXT("skip_routing_unchanged"));
				return true;
			}
		}
	}

	return false;
}


bool AStructuralPowerGraphSubsystem::ShouldSkipSwitchCircuitEcho(
	AFGBuildableCircuitSwitch* Switch,
	const TCHAR** OutReason)
{
	auto SetReason = [OutReason](const TCHAR* Reason)
	{
		if (OutReason)
		{
			*OutReason = Reason;
		}
	};

	if (!IsValid(Switch))
	{
		return false;
	}

	const FStructuralNodeId SwitchId = MakeNodeId(Switch);
	const FTrackedEndpoint* Tracked = TrackedEndpoints.Find(SwitchId);
	FStructuralNodeId ParentId;
	int32 Root = INDEX_NONE;
	if (Tracked && Tracked->ParentId.IsValid())
	{
		ParentId = Tracked->ParentId;
		Root = StructureGraph.FindRoot(ParentId);
	}
	else
	{
		FStructuralWallAnchor ParentAnchor = ResolveOutletAnchor(Switch);
		Root = BridgeRootIndex.ResolveEndpointComponentRoot(Switch, ParentAnchor, ParentId);
	}

	if (SiteState.IsEchoScopeActive())
	{
		if (!SiteState.IsEchoDirtySite(Root))
		{
			SetReason(TEXT("skip_echo_scope"));
			return true;
		}

		if (SiteState.WasSwitchToggleHandledInScope(SwitchId))
		{
			SetReason(TEXT("skip_echo_toggle"));
			return true;
		}

		if (SiteState.WasSwitchEchoProcessedInScope(SwitchId))
		{
			SetReason(TEXT("skip_echo_coalesce"));
			return true;
		}
	}

	return false;
}


void AStructuralPowerGraphSubsystem::MarkEchoDirtyForSwitchToggle(
	AFGBuildableCircuitSwitch* Switch,
	int32 LocalRoot)
{
	TArray<int32> DirtySites;
	if (LocalRoot != INDEX_NONE)
	{
		DirtySites.Add(LocalRoot);
	}

	if (FStructuralPowerModConfig::IsGroupLightingEnabled()
		&& IsValid(Switch)
		&& FStructuralSwitchParentResolver::IsWiredToStructureSide(Switch))
	{
		CrossSiteGraph.RefreshCouplingsFromWiredSwitch(*this, Switch, LocalRoot);

		TArray<int32> FeedAffectedSites;
		CrossSiteGraph.TraceFeedAffected(*this, Switch, LocalRoot, FeedAffectedSites);
		for (int32 Site : FeedAffectedSites)
		{
			DirtySites.AddUnique(Site);
		}
	}

	if (DirtySites.Num() == 0)
	{
		return;
	}

	SiteState.BeginEchoScope(DirtySites);
}


void AStructuralPowerGraphSubsystem::NotePanelCircuitEchoProcessed(
	AFGBuildableLightsControlPanel* Panel)
{
	if (!IsValid(Panel))
	{
		return;
	}

	SiteState.NotePanelEchoProcessed(MakeNodeId(Panel));
}


void AStructuralPowerGraphSubsystem::NotePanelToggleHandled(
	AFGBuildableLightsControlPanel* Panel)
{
	if (!IsValid(Panel))
	{
		return;
	}

	SiteState.NotePanelToggleHandled(MakeNodeId(Panel));
}


void AStructuralPowerGraphSubsystem::NoteSwitchCircuitEchoProcessed(
	AFGBuildableCircuitSwitch* Switch)
{
	if (!IsValid(Switch))
	{
		return;
	}

	SiteState.NoteSwitchEchoProcessed(MakeNodeId(Switch));
}


void AStructuralPowerGraphSubsystem::NoteSwitchToggleHandled(
	AFGBuildableCircuitSwitch* Switch)
{
	if (!IsValid(Switch))
	{
		return;
	}

	SiteState.NoteSwitchToggleHandled(MakeNodeId(Switch));
}


void AStructuralPowerGraphSubsystem::ProcessPoleWireDelta(AFGBuildablePowerPole* Pole)
{
	WireDeltaHandler.ProcessPoleWireDelta(Pole);
}


void AStructuralPowerGraphSubsystem::ProcessPoleEndpoint(AFGBuildablePowerPole* Pole)
{
	FStructuralPowerContext Ctx = GetProcessorContext();
	if (IStructuralPowerProcessor* Processor =
			FStructuralPowerProcessorRegistry::Get().FindMutable(EStructuralEndpointKind::Pole))
	{
		Processor->Process(Ctx, Pole);
	}
}

void AStructuralPowerGraphSubsystem::ProcessStorageEndpoint(AFGBuildablePowerStorage* Storage)
{
	FStructuralPowerContext Ctx = GetProcessorContext();
	if (IStructuralPowerProcessor* Processor =
			FStructuralPowerProcessorRegistry::Get().FindMutable(EStructuralEndpointKind::Storage))
	{
		Processor->Process(Ctx, Storage);
	}
}

void AStructuralPowerGraphSubsystem::ProcessPoleEndpointDirect(AFGBuildablePowerPole* Pole)
{
	FStructuralPowerContext Ctx = GetProcessorContext();
	FStructuralPowerPoleProcessor::Process(Ctx, Pole);
}

void AStructuralPowerGraphSubsystem::TearDownLightStructuralLinks(AFGBuildableLightSource* Light)
{
	FStructuralPowerContext Ctx = GetProcessorContext();
	if (IStructuralPowerProcessor* Processor =
			FStructuralPowerProcessorRegistry::Get().FindMutable(EStructuralEndpointKind::Light))
	{
		Processor->TearDown(Ctx, Light);
	}
}

void AStructuralPowerGraphSubsystem::ProcessLightEndpoint(
	AFGBuildableLightSource* Light,
	bool bLocalPromoteOnly)
{
	FStructuralPowerContext Ctx = GetProcessorContext();
	if (IStructuralPowerProcessor* Processor =
			FStructuralPowerProcessorRegistry::Get().FindMutable(EStructuralEndpointKind::Light))
	{
		Processor->Process(Ctx, Light, bLocalPromoteOnly);
	}
}

void AStructuralPowerGraphSubsystem::TearDownPanelStructuralLinks(
	AFGBuildableLightsControlPanel* Panel)
{
	FStructuralPowerContext Ctx = GetProcessorContext();
	if (IStructuralPowerProcessor* Processor =
			FStructuralPowerProcessorRegistry::Get().FindMutable(EStructuralEndpointKind::Panel))
	{
		Processor->TearDown(Ctx, Panel);
	}
}

void AStructuralPowerGraphSubsystem::ProcessPanelEndpoint(
	AFGBuildableLightsControlPanel* Panel,
	bool bLocalPromoteOnly)
{
	FStructuralPowerContext Ctx = GetProcessorContext();
	if (IStructuralPowerProcessor* Processor =
			FStructuralPowerProcessorRegistry::Get().FindMutable(EStructuralEndpointKind::Panel))
	{
		Processor->Process(Ctx, Panel, bLocalPromoteOnly);
	}
}

void AStructuralPowerGraphSubsystem::ProcessGeneratorEndpoint(AFGBuildableGenerator* Generator)
{
	FStructuralPowerContext Ctx = GetProcessorContext();
	FStructuralPowerGeneratorProcessor::Process(Ctx, Generator);
}

void AStructuralPowerGraphSubsystem::EnumerateTrackedLightsOnRoot(
	int32 Root,
	TFunctionRef<void(AFGBuildableLightSource*)> Visitor)
{
	ReconcileOps.EnumerateTrackedLightsOnRoot(Root, Visitor);
}


void AStructuralPowerGraphSubsystem::ReconcileAllLightConsumers()
{
	ReconcileOps.ReconcileAllLightConsumers();
}

void AStructuralPowerGraphSubsystem::ReconcileGroupLightingState()
{
	ReconcileOps.ReconcileGroupLightingState();
}

void AStructuralPowerGraphSubsystem::RefreshPanelsForLightSourceOnRoot(
	int32 Root,
	FName LightSource)
{
	ReconcileOps.RefreshPanelsForLightSourceOnRoot(Root, LightSource);
}


void AStructuralPowerGraphSubsystem::ProcessWallOutletAfterWire(AFGBuildablePowerPole* Pole)
{
	WireDeltaHandler.ProcessWallOutletAfterWire(Pole);
}


void AStructuralPowerGraphSubsystem::OnBuildableRemoved(AFGBuildable* Buildable)
{
	if (!IsValid(Buildable))
	{
		return;
	}

	const FStructuralNodeId NodeId = MakeNodeId(Buildable);
	int32 GangRoot = INDEX_NONE;
	if (const FTrackedEndpoint* TrackedBefore = TrackedEndpoints.Find(NodeId))
	{
		GangRoot = StructureGraph.FindRoot(TrackedBefore->ParentId);
	}

	ControlIdGangIndex.RemoveNode(NodeId);
	UnregisterBuildableActor(NodeId);

	if (FTrackedEndpoint* Tracked = TrackedEndpoints.Find(NodeId))
	{
		const int32 OldRoot = StructureGraph.FindRoot(Tracked->ParentId);

		if (AFGBuildable* Host = Tracked->Actor.Get())
		{
			if (Tracked->Kind == EStructuralEndpointKind::Switch)
			{
				FStructuralPowerContext Ctx = GetProcessorContext();
				if (AFGBuildableCircuitSwitch* Switch = Tracked->GetSwitch())
				{
					FStructuralPowerSwitchProcessor::TearDown(Ctx, Switch);
				}
			}
			else if (Tracked->Kind == EStructuralEndpointKind::Light)
			{
				if (AFGBuildableLightSource* Light = Tracked->GetLight())
				{
					TearDownLightStructuralLinks(Light);
				}
			}
			else if (Tracked->Kind == EStructuralEndpointKind::Panel)
			{
				if (AFGBuildableLightsControlPanel* Panel = Tracked->GetPanel())
				{
					TearDownPanelStructuralLinks(Panel);
				}
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
	}
	else if (StructureGraph.IsTracked(NodeId))
	{
		TArray<int32> AffectedRoots;
		StructureGraph.RemoveNode(NodeId, AffectedRoots);
	}

	PlacementQueue.RemoveBuildable(Buildable);

	if (GangRoot != INDEX_NONE)
	{
		RebuildControlIdGangsForRoot(GangRoot);
	}
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
			|| FStructuralEligibilityRules::IsPowerBridgeSwitch(Buildable))
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
