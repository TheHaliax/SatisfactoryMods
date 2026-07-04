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

void UStructuralPowerRootInstanceModule::UnregisterGlobalDelegates()
{
	if (PostLoadMapHandle.IsValid())
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
		PostLoadMapHandle.Reset();
	}
}

bool UStructuralPowerRootInstanceModule::TryEnqueueBuildable(
	AFGBuildable* Buildable,
	const TCHAR* HookName,
	const TCHAR* SourceTag)
{
	if (!IsValid(Buildable))
	{
		return false;
	}

	if (!Buildable->HasAuthority())
	{
		FStructuralPowerTrace::LogPlacementSkip(Buildable, TEXT("no_authority"));
		return false;
	}

	UWorld* World = Buildable->GetWorld();
	if (!IsValid(World))
	{
		FStructuralPowerTrace::LogPlacementSkip(Buildable, TEXT("no_world"));
		return false;
	}

	AStructuralPowerGraphSubsystem* Graph = AStructuralPowerGraphSubsystem::GetOrCreate(World);
	if (!Graph)
	{
		return false;
	}

	if (FStructuralEligibilityRules::IsPowerBridgePole(Buildable))
	{
		FStructuralPowerTrace::LogHook(Buildable, HookName, TEXT("enqueue_bridge_pole"), SourceTag);
		Graph->EnqueuePlacement(Buildable, EStructuralPlacementJobType::Outlet, /*bDefer=*/true);
		return true;
	}

	if (FStructuralEligibilityRules::IsBusMember(Buildable))
	{
		FStructuralPowerTrace::LogHook(Buildable, HookName, TEXT("enqueue_structure"), SourceTag);
		Graph->EnqueuePlacement(Buildable, EStructuralPlacementJobType::Structure, /*bDefer=*/true);
		return true;
	}

	FStructuralPowerTrace::LogHook(Buildable, HookName, TEXT("ignored"), TEXT("not_bus_or_outlet"));
	return false;
}

void UStructuralPowerRootInstanceModule::HandleBuildableBuilt(AFGBuildable* Buildable)
{
	TryEnqueueBuildable(Buildable, TEXT("OnBuildEffectFinished"), TEXT("defer"));
}

void UStructuralPowerRootInstanceModule::HandleBuildablesConstructed(const TArray<AActor*>& Children)
{
	int32 Enqueued = 0;
	for (AActor* Child : Children)
	{
		if (AFGBuildable* Buildable = Cast<AFGBuildable>(Child))
		{
			if (TryEnqueueBuildable(Buildable, TEXT("BlueprintConstruct"), TEXT("defer")))
			{
				++Enqueued;
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
	if (!IsValid(Pole) || !Pole->HasAuthority())
	{
		return;
	}

	if (!FStructuralEligibilityRules::IsPowerBridgePole(Pole))
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
		if (AStructuralPowerGraphSubsystem* Graph = AStructuralPowerGraphSubsystem::Find(World))
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
		if (AStructuralPowerGraphSubsystem* Graph = AStructuralPowerGraphSubsystem::Find(World))
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
				AStructuralPowerGraphSubsystem* Graph = AStructuralPowerGraphSubsystem::GetOrCreate(WorldPtr);
				UStructuralPowerFactoryTickHandler::RegisterForWorld(WorldPtr);
				if (Graph)
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
	UE_LOG(LogStructuralPower, Log, TEXT("StructuralPower: skipping hooks in editor"));
#else
	UE_LOG(LogStructuralPower, Log, TEXT("StructuralPower v2.0 initialized — pole-to-pole bus over a structural connectivity graph"));

	SUBSCRIBE_METHOD_VIRTUAL_AFTER(
		AFGBuildable::OnBuildEffectFinished,
		GetMutableDefault<AFGBuildable>(),
		[](AFGBuildable* Buildable)
		{
			HandleBuildableBuilt(Buildable);
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
#endif

	Super::DispatchLifecycleEvent(Phase);
}
