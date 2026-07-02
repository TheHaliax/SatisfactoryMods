// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "StructuralPowerRootInstanceModule.h"

#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "Diagnostics/FStructuralPowerTrace.h"
#include "FGLightweightBuildableSubsystem.h"
#include "FGBuildableSubsystem.h"
#include "Hologram/FGBlueprintHologram.h"
#include "Lightweight/FStructuralLightweightTypes.h"
#include "Patching/NativeHookManager.h"
#include "Rules/FStructuralEligibilityRules.h"
#include "Save/AStructuralPowerGraphSubsystem.h"
#include "Subsystems/UStructuralPowerFactoryTickHandler.h"
#include "StructuralPowerLog.h"
#include "TimerManager.h"

FDelegateHandle UStructuralPowerRootInstanceModule::PostLoadMapHandle;

UStructuralPowerRootInstanceModule::UStructuralPowerRootInstanceModule()
{
	bRootModule = true;
}

void UStructuralPowerRootInstanceModule::HandleBuildableBuilt(AFGBuildable* Buildable)
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

	UWorld* World = Buildable->GetWorld();
	if (!IsValid(World))
	{
		FStructuralPowerTrace::LogPlacementSkip(Buildable, TEXT("no_world"));
		return;
	}

	if (AStructuralPowerGraphSubsystem* Graph = AStructuralPowerGraphSubsystem::GetOrCreate(World))
	{
		if (FStructuralEligibilityRules::IsPowerBridgePole(Buildable))
		{
			FStructuralPowerTrace::LogHook(Buildable, TEXT("OnBuildEffectFinished"), TEXT("enqueue_bridge_pole"), TEXT("defer"));
			Graph->EnqueuePlacement(Buildable, EStructuralPlacementJobType::Outlet, /*bDefer=*/true);
		}
		else if (FStructuralEligibilityRules::IsBusMember(Buildable))
		{
			FStructuralPowerTrace::LogHook(Buildable, TEXT("OnBuildEffectFinished"), TEXT("enqueue_structure"), TEXT("defer"));
			Graph->EnqueuePlacement(Buildable, EStructuralPlacementJobType::Structure, /*bDefer=*/true);
		}
		else
		{
			FStructuralPowerTrace::LogHook(Buildable, TEXT("OnBuildEffectFinished"), TEXT("ignored"), TEXT("not_bus_or_outlet"));
		}
	}
}

void UStructuralPowerRootInstanceModule::HandleBuildablesConstructed(const TArray<AActor*>& Children)
{
	int32 Enqueued = 0;
	for (AActor* Child : Children)
	{
		if (AFGBuildable* Buildable = Cast<AFGBuildable>(Child))
		{
			if (!IsValid(Buildable) || !Buildable->HasAuthority())
			{
				continue;
			}

			UWorld* World = Buildable->GetWorld();
			if (!IsValid(World))
			{
				continue;
			}

			if (AStructuralPowerGraphSubsystem* Graph = AStructuralPowerGraphSubsystem::GetOrCreate(World))
			{
				if (FStructuralEligibilityRules::IsPowerBridgePole(Buildable))
				{
					FStructuralPowerTrace::LogHook(Buildable, TEXT("BlueprintConstruct"), TEXT("enqueue_bridge_pole"), TEXT("defer"));
					Graph->EnqueuePlacement(Buildable, EStructuralPlacementJobType::Outlet, /*bDefer=*/true);
					++Enqueued;
				}
				else if (FStructuralEligibilityRules::IsBusMember(Buildable))
				{
					FStructuralPowerTrace::LogHook(Buildable, TEXT("BlueprintConstruct"), TEXT("enqueue_structure"), TEXT("defer"));
					Graph->EnqueuePlacement(Buildable, EStructuralPlacementJobType::Structure, /*bDefer=*/true);
					++Enqueued;
				}
			}
		}
	}

	if (Enqueued > 0)
	{
		UE_LOG(LogStructuralPower, Log, TEXT("[PWR] BlueprintConstruct deferred %d buildable(s)"), Enqueued);
	}
}

void UStructuralPowerRootInstanceModule::HandlePowerConnectionChanged(
	AFGBuildablePowerPole* Pole,
	UFGCircuitConnectionComponent* /*Connection*/)
{
	if (!FStructuralEligibilityRules::IsPowerBridgePole(Pole))
	{
		return;
	}

	if (!IsValid(Pole) || !Pole->HasAuthority())
	{
		return;
	}

	UWorld* World = Pole->GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda(
		[WorldWeak = TWeakObjectPtr<UWorld>(World), PoleWeak = TWeakObjectPtr<AFGBuildablePowerPole>(Pole)]()
		{
			if (AFGBuildablePowerPole* PolePtr = PoleWeak.Get())
			{
				if (UWorld* WorldPtr = WorldWeak.Get())
				{
					if (AStructuralPowerGraphSubsystem* Graph = AStructuralPowerGraphSubsystem::GetOrCreate(WorldPtr))
					{
						Graph->ProcessWallOutletAfterWire(PolePtr);
					}
				}
			}
		}));
}

void UStructuralPowerRootInstanceModule::HandleBuildableRemoved(AFGBuildable* Buildable)
{
	if (!IsValid(Buildable))
	{
		return;
	}

	if (UWorld* World = Buildable->GetWorld())
	{
		if (AStructuralPowerGraphSubsystem* Graph = AStructuralPowerGraphSubsystem::GetOrCreate(World))
		{
			Graph->OnBuildableRemoved(Buildable);
		}
	}
}

void UStructuralPowerRootInstanceModule::HandleLightweightMemberAdded(
	AFGLightweightBuildableSubsystem* Subsystem,
	TSubclassOf<AFGBuildable> BuildableClass,
	int32 InstanceIndex)
{
	if (!IsValid(Subsystem) || InstanceIndex == INDEX_NONE || !BuildableClass)
	{
		return;
	}

	const AFGBuildable* CDO = BuildableClass->GetDefaultObject<AFGBuildable>();
	if (!FStructuralEligibilityRules::IsBusMember(CDO))
	{
		return;
	}

	UWorld* World = Subsystem->GetWorld();
	if (!IsValid(World) || World->GetNetMode() == NM_Client)
	{
		return;
	}

	const FStructuralLightweightKey Key{BuildableClass, InstanceIndex};
	UE_LOG(LogStructuralPower, Log,
		TEXT("[PWR] LW hook AddFromBuildableInstanceData %s[%d]"),
		*BuildableClass->GetName(),
		InstanceIndex);

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda(
		[WorldWeak = TWeakObjectPtr<UWorld>(World), Key]()
		{
			if (UWorld* WorldPtr = WorldWeak.Get())
			{
				if (AStructuralPowerGraphSubsystem* Graph = AStructuralPowerGraphSubsystem::GetOrCreate(WorldPtr))
				{
					Graph->EnqueueLightweightPlacement(Key, /*bDefer=*/true);
				}
			}
		}));
}

void UStructuralPowerRootInstanceModule::HandleLightweightMemberRemoved(
	AFGLightweightBuildableSubsystem* Subsystem,
	TSubclassOf<AFGBuildable> BuildableClass,
	int32 InstanceIndex)
{
	if (!IsValid(Subsystem) || InstanceIndex == INDEX_NONE || !BuildableClass)
	{
		return;
	}

	if (UWorld* World = Subsystem->GetWorld())
	{
		if (AStructuralPowerGraphSubsystem* Graph = AStructuralPowerGraphSubsystem::GetOrCreate(World))
		{
			Graph->OnLightweightRemoved(FStructuralLightweightKey{BuildableClass, InstanceIndex});
		}
	}
}

void UStructuralPowerRootInstanceModule::HandlePostLoadMap(UWorld* World)
{
	if (!IsValid(World) || !World->IsGameWorld())
	{
		return;
	}

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda(
		[WorldWeak = TWeakObjectPtr<UWorld>(World)]()
		{
			if (UWorld* WorldPtr = WorldWeak.Get())
			{
				AStructuralPowerGraphSubsystem::GetOrCreate(WorldPtr);
				UStructuralPowerFactoryTickHandler::RegisterForWorld(WorldPtr);
				if (AStructuralPowerGraphSubsystem* Graph = AStructuralPowerGraphSubsystem::GetOrCreate(WorldPtr))
				{
					Graph->OnWorldReady(WorldPtr);
				}
			}
		}));
}

void UStructuralPowerRootInstanceModule::DispatchLifecycleEvent(ELifecyclePhase Phase)
{
	if (Phase != ELifecyclePhase::INITIALIZATION)
	{
		Super::DispatchLifecycleEvent(Phase);
		return;
	}

#if WITH_EDITOR
	UE_LOG(LogStructuralPower, Log, TEXT("StructuralPower v3: skipping hooks in editor"));
	Super::DispatchLifecycleEvent(Phase);
	return;
#endif

	UE_LOG(LogStructuralPower, Log, TEXT("StructuralPower v1.0 initialized — placement-only structural bus (actor + lightweight)"));

	SUBSCRIBE_METHOD_VIRTUAL_AFTER(
		AFGBuildable::OnBuildEffectFinished,
		GetMutableDefault<AFGBuildable>(),
		[](AFGBuildable* Buildable)
		{
			HandleBuildableBuilt(Buildable);
		});

	SUBSCRIBE_METHOD_VIRTUAL_AFTER(
		AFGBuildablePowerPole::OnBuildEffectFinished,
		GetMutableDefault<AFGBuildablePowerPole>(),
		[](AFGBuildablePowerPole* Pole)
		{
			HandleBuildableBuilt(Pole);
		});

	SUBSCRIBE_METHOD_VIRTUAL_AFTER(
		AFGBlueprintHologram::Construct,
		GetMutableDefault<AFGBlueprintHologram>(),
		[](AActor* /*ReturnValue*/, AFGBlueprintHologram* /*Hologram*/, TArray<AActor*>& OutChildren, FNetConstructionID /*NetConstructionID*/)
		{
			HandleBuildablesConstructed(OutChildren);
		});

	SUBSCRIBE_METHOD_AFTER(
		AFGBuildableSubsystem::RemoveBuildable,
		[](AFGBuildableSubsystem* /*Subsystem*/, AFGBuildable* Buildable)
		{
			HandleBuildableRemoved(Buildable);
		});

	SUBSCRIBE_METHOD_VIRTUAL_AFTER(
		AFGBuildablePowerPole::OnPowerConnectionChanged,
		GetMutableDefault<AFGBuildablePowerPole>(),
		[](AFGBuildablePowerPole* Pole, UFGCircuitConnectionComponent* Connection)
		{
			HandlePowerConnectionChanged(Pole, Connection);
		});

	SUBSCRIBE_METHOD_AFTER(
		AFGLightweightBuildableSubsystem::AddFromBuildableInstanceData,
		[](int32 ReturnValue,
			AFGLightweightBuildableSubsystem* Subsystem,
			TSubclassOf<AFGBuildable> BuildableClass,
			FRuntimeBuildableInstanceData& /*BuildableInstanceData*/,
			bool FromSaveData,
			int32 /*SaveDataBuildableIndex*/,
			uint16 /*ConstructId*/,
			AActor* /*BuildEffectInstigator*/,
			int32 /*BlueprintBuildEffectIndex*/)
		{
			if (FromSaveData)
			{
				return;
			}

			HandleLightweightMemberAdded(Subsystem, BuildableClass, ReturnValue);
		});

	SUBSCRIBE_METHOD_AFTER(
		AFGLightweightBuildableSubsystem::RemoveByInstanceIndex,
		[](AFGLightweightBuildableSubsystem* Subsystem,
			TSubclassOf<AFGBuildable> BuildableClass,
			int32 InstanceIndex)
		{
			HandleLightweightMemberRemoved(Subsystem, BuildableClass, InstanceIndex);
		});

	if (!PostLoadMapHandle.IsValid())
	{
		PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddStatic(&HandlePostLoadMap);
	}

	Super::DispatchLifecycleEvent(Phase);
}
