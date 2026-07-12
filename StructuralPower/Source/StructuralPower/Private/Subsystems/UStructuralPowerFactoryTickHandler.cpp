// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Subsystems/UStructuralPowerFactoryTickHandler.h"

#include "Core/FStructuralPowerWorldGate.h"
#include "Engine/World.h"
#include "FGBuildableSubsystem.h"
#include "Save/AStructuralPowerGraphSubsystem.h"
#include "StructuralPowerConstants.h"

TMap<TWeakObjectPtr<UWorld>, TObjectPtr<UStructuralPowerFactoryTickHandler>>
	UStructuralPowerFactoryTickHandler::Handlers;
FDelegateHandle UStructuralPowerFactoryTickHandler::WorldCleanupHandle;

void UStructuralPowerFactoryTickHandler::UnregisterGlobalDelegates()
{
	TArray<UWorld*> WorldsToUnregister;
	WorldsToUnregister.Reserve(Handlers.Num());
	for (const TPair<TWeakObjectPtr<UWorld>, TObjectPtr<UStructuralPowerFactoryTickHandler>>& Pair : Handlers)
	{
		if (UWorld* World = Pair.Key.Get())
		{
			WorldsToUnregister.Add(World);
		}
	}

	for (UWorld* World : WorldsToUnregister)
	{
		UnregisterForWorld(World);
	}

	Handlers.Empty();

	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		WorldCleanupHandle.Reset();
	}
}

void UStructuralPowerFactoryTickHandler::HandleWorldCleanup(
	UWorld* World,
	bool /*bSessionEnded*/,
	bool /*bCleanupResources*/)
{
	UnregisterForWorld(World);
}

void UStructuralPowerFactoryTickHandler::RegisterForWorld(UWorld* World)
{
	if (!IsValid(World))
	{
		return;
	}

	if (!WorldCleanupHandle.IsValid())
	{
		WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddStatic(&HandleWorldCleanup);
	}

	for (auto It = Handlers.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}

	const TWeakObjectPtr<UWorld> WorldKey(World);
	if (Handlers.Contains(WorldKey))
	{
		return;
	}

	AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(World);
	if (!IsValid(BuildableSubsystem))
	{
		return;
	}

	UStructuralPowerFactoryTickHandler* Handler = NewObject<UStructuralPowerFactoryTickHandler>(World);
	BuildableSubsystem->AddFactoryTickHandler(Handler);
	Handlers.Add(WorldKey, Handler);
}

void UStructuralPowerFactoryTickHandler::UnregisterForWorld(UWorld* World)
{
	if (!IsValid(World))
	{
		return;
	}

	const TWeakObjectPtr<UWorld> WorldKey(World);
	if (TObjectPtr<UStructuralPowerFactoryTickHandler>* Found = Handlers.Find(WorldKey))
	{
		if (AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(World))
		{
			if (IsValid(*Found))
			{
				BuildableSubsystem->RemoveFactoryTickHandler(*Found);
			}
		}
		Handlers.Remove(WorldKey);
	}
}

void UStructuralPowerFactoryTickHandler::PostFactoryTick(
	AFGBuildableSubsystem* /*Subsystem*/,
	float /*DeltaTime*/)
{
	UWorld* World = GetWorld();
	if (!FStructuralPowerWorldGate::IsGameplayWorld(World))
	{
		UnregisterForWorld(World);
		return;
	}

	if (AStructuralPowerGraphSubsystem* Graph = AStructuralPowerGraphSubsystem::Find(World))
	{
		if (Graph->GetPendingJobCount() > 0)
		{
			Graph->TickDeferredPlacements(StructuralPowerConstants::DeferredPlacementsPerTick);
		}
		else
		{
			Graph->MaybeReleaseFactoryTick();
		}
	}
	else
	{
		UnregisterForWorld(World);
	}
}
