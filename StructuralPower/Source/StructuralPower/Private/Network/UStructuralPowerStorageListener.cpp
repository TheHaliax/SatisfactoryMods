// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Network/UStructuralPowerStorageListener.h"

#include "Buildables/FGBuildablePowerStorage.h"
#include "Connection/FStructuralStorageConnectionPoint.h"
#include "Core/EAttachContext.h"
#include "Core/FStructuralPowerContext.h"
#include "FGCircuitConnectionComponent.h"
#include "FGPowerConnectionComponent.h"
#include "Save/AStructuralPowerGraphSubsystem.h"

void UStructuralPowerStorageListener::BindSubsystem(
	AStructuralPowerGraphSubsystem* Graph,
	AFGBuildablePowerStorage* Storage)
{
	if (!IsValid(Graph) || !IsValid(Storage))
	{
		return;
	}

	GraphSubsystem = Graph;
	BoundStorage = Storage;

	TInlineComponentArray<UFGPowerConnectionComponent*> PowerConns;
	Storage->GetComponents(PowerConns);
	for (UFGPowerConnectionComponent* Conn : PowerConns)
	{
		if (!IsValid(Conn) || Conn->IsHidden())
		{
			continue;
		}

		Conn->OnConnectionChanged.AddWeakLambda(
			this,
			[this](UFGCircuitConnectionComponent* Connection)
			{
				HandleConnectionChanged(Connection);
			});
		BoundConns.Add(Conn);
	}
}

void UStructuralPowerStorageListener::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (const TWeakObjectPtr<UFGCircuitConnectionComponent>& WeakConn : BoundConns)
	{
		if (UFGCircuitConnectionComponent* Conn = WeakConn.Get())
		{
			Conn->OnConnectionChanged.RemoveAll(this);
		}
	}
	BoundConns.Reset();

	Super::EndPlay(EndPlayReason);
}

void UStructuralPowerStorageListener::HandleConnectionChanged(
	UFGCircuitConnectionComponent* /*Connection*/)
{
	AStructuralPowerGraphSubsystem* Graph = GraphSubsystem.Get();
	AFGBuildablePowerStorage* Storage = BoundStorage.Get();
	if (!IsValid(Graph) || !IsValid(Storage))
	{
		return;
	}

	if (Graph->ShouldDeferCircuitDrivenRefresh() || Graph->IsBuildablePlacementPending(Storage))
	{
		return;
	}

	FStructuralStorageConnectionPoint(*Graph, Storage).OnWireOrGateChanged(EAttachContext::WireDelta);
}
