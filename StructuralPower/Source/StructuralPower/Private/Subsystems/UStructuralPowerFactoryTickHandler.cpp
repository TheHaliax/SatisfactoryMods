// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Subsystems/UStructuralPowerFactoryTickHandler.h"

#include "Engine/World.h"
#include "FGBuildableSubsystem.h"
#include "Save/AStructuralPowerGraphSubsystem.h"
#include "StructuralPowerConstants.h"

TMap<TWeakObjectPtr<UWorld>, TObjectPtr<UStructuralPowerFactoryTickHandler>>
	UStructuralPowerFactoryTickHandler::Handlers;

void UStructuralPowerFactoryTickHandler::RegisterForWorld(UWorld* World)
{
	if (!IsValid(World))
	{
		return;
	}

	for (auto It = Handlers.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}

	if (Handlers.Contains(World))
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
	Handlers.Add(World, Handler);
}

void UStructuralPowerFactoryTickHandler::UnregisterForWorld(UWorld* World)
{
	if (!IsValid(World))
	{
		return;
	}

	if (TObjectPtr<UStructuralPowerFactoryTickHandler>* Found = Handlers.Find(World))
	{
		if (AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(World))
		{
			if (IsValid(*Found))
			{
				BuildableSubsystem->RemoveFactoryTickHandler(*Found);
			}
		}
		Handlers.Remove(World);
	}
}

void UStructuralPowerFactoryTickHandler::PostFactoryTick(
	AFGBuildableSubsystem* /*Subsystem*/,
	float /*DeltaTime*/)
{
	if (UWorld* World = GetWorld())
	{
		if (AStructuralPowerGraphSubsystem* Graph = AStructuralPowerGraphSubsystem::GetOrCreate(World))
		{
			Graph->TickDeferredPlacements(StructuralPowerConstants::DeferredPlacementsPerTick);
		}
	}
}
