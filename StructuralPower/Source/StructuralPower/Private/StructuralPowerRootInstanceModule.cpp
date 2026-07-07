// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "StructuralPowerRootInstanceModule.h"

#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableCircuitBridge.h"
#include "Buildables/FGBuildableControlPanelHost.h"
#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Buildables/FGBuildableLightSource.h"
#include "Buildables/FGBuildableLightsControlPanel.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "Config/FStructuralPowerModConfig.h"
#include "Config/UStructuralPowerModConfiguration.h"
#include "Diagnostics/FStructuralPowerTrace.h"
#include "FGLightweightBuildableSubsystem.h"
#include "FGBuildableSubsystem.h"
#include "Hologram/FGBlueprintHologram.h"
#include "Input/FStructuralPowerIdInput.h"
#include "Lightweight/FStructuralLightweightTypes.h"
#include "Network/UStructuralPowerRCO.h"
#include "Patching/NativeHookManager.h"
#include "Equipment/FStructuralEquipmentBridgeRegistry.h"
#include "Processors/FStructuralPowerProcessorRegistry.h"
#include "FGHUD.h"
#include "Rules/FStructuralEligibilityRules.h"
#include "Panel/FStructuralPanelControlledSync.h"
#include "Routing/EStructuralChannel.h"
#include "Save/AStructuralPowerGraphSubsystem.h"
#include "StructuralPowerConstants.h"
#include "Subsystems/UStructuralPowerFactoryTickHandler.h"
#include "StructuralPowerLog.h"
#include "TimerManager.h"
#include "UI/FGGameUI.h"
#include "UI/FGInteractWidget.h"
#include "UI/FStructuralPowerIdPresenterFactory.h"

FDelegateHandle UStructuralPowerRootInstanceModule::PostLoadMapHandle;

UStructuralPowerRootInstanceModule::UStructuralPowerRootInstanceModule()
{
	bRootModule = true;
	RemoteCallObjects.Add(UStructuralPowerRCO::StaticClass());
	ModConfigurations.Add(UStructuralPowerModConfiguration::StaticClass());
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

	if (FStructuralPowerModConfig::IsGatePowerSwitchesEnabled()
		&& FStructuralEligibilityRules::IsPowerBridgeSwitch(Buildable))
	{
		FStructuralPowerTrace::LogHook(Buildable, HookName, TEXT("enqueue_switch"), SourceTag);
		Graph->EnqueuePlacement(Buildable, EStructuralPlacementJobType::Outlet, /*bDefer=*/true);
		return true;
	}

	if (FStructuralEligibilityRules::IsStructuralLightConsumer(Buildable))
	{
		FStructuralPowerTrace::LogHook(Buildable, HookName, TEXT("enqueue_light"), SourceTag);
		Graph->EnqueuePlacement(Buildable, EStructuralPlacementJobType::Outlet, /*bDefer=*/true);
		return true;
	}

	if (Buildable->IsA<AFGBuildableLightsControlPanel>())
	{
		FStructuralPowerTrace::LogHook(Buildable, HookName, TEXT("enqueue_panel"), SourceTag);
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

static void HandleSwitchBeginPlay(AFGBuildableCircuitSwitch* Switch)
{
	if (!IsValid(Switch) || !Switch->HasAuthority())
	{
		return;
	}

	if (!FStructuralPowerModConfig::IsGatePowerSwitchesEnabled())
	{
		return;
	}

	UWorld* World = Switch->GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	UE_LOG(LogStructuralPower, Log,
		TEXT("[HALSP] switch BeginPlay %s — enqueue outlet"),
		*Switch->GetName());

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda(
		[WorldWeak = TWeakObjectPtr<UWorld>(World),
			SwitchWeak = TWeakObjectPtr<AFGBuildableCircuitSwitch>(Switch)]()
		{
			if (AFGBuildableCircuitSwitch* SwitchPtr = SwitchWeak.Get())
			{
				if (UWorld* WorldPtr = WorldWeak.Get())
				{
					if (AStructuralPowerGraphSubsystem* Graph =
						AStructuralPowerGraphSubsystem::GetOrCreate(WorldPtr))
					{
						Graph->EnqueuePlacement(
							SwitchPtr,
							EStructuralPlacementJobType::Outlet,
							/*bDefer=*/true);
					}
				}
			}
		}));
}

static void EnqueuePanelOutletWhenReady(
	TWeakObjectPtr<UWorld> WorldWeak,
	TWeakObjectPtr<AFGBuildableLightsControlPanel> PanelWeak)
{
	AFGBuildableLightsControlPanel* PanelPtr = PanelWeak.Get();
	UWorld* WorldPtr = WorldWeak.Get();
	if (!IsValid(PanelPtr) || !IsValid(WorldPtr))
	{
		return;
	}

	AStructuralPowerGraphSubsystem* Graph = AStructuralPowerGraphSubsystem::GetOrCreate(WorldPtr);
	if (!Graph)
	{
		return;
	}

	if (Graph->IsBulkLoadDrainActive())
	{
		WorldPtr->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateStatic(&EnqueuePanelOutletWhenReady, WorldWeak, PanelWeak));
		return;
	}

	Graph->EnqueuePlacement(
		PanelPtr,
		EStructuralPlacementJobType::Outlet,
		/*bDefer=*/true);
}

static void HandlePanelBeginPlay(AFGBuildableLightsControlPanel* Panel)
{
	if (!IsValid(Panel) || !Panel->HasAuthority())
	{
		return;
	}

	UWorld* World = Panel->GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	UE_LOG(LogStructuralPower, Log,
		TEXT("[HALSP] panel BeginPlay %s — enqueue outlet"),
		*Panel->GetName());

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateStatic(
		&EnqueuePanelOutletWhenReady,
		TWeakObjectPtr<UWorld>(World),
		TWeakObjectPtr<AFGBuildableLightsControlPanel>(Panel)));
}

static void HandleLightBeginPlay(AFGBuildableLightSource* Light)
{
	if (!IsValid(Light) || !Light->HasAuthority())
	{
		return;
	}

	UWorld* World = Light->GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	UE_LOG(LogStructuralPower, Log,
		TEXT("[HALSP] light BeginPlay %s — enqueue outlet"),
		*Light->GetName());

	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda(
		[WorldWeak = TWeakObjectPtr<UWorld>(World),
			LightWeak = TWeakObjectPtr<AFGBuildableLightSource>(Light)]()
		{
			if (AFGBuildableLightSource* LightPtr = LightWeak.Get())
			{
				if (UWorld* WorldPtr = WorldWeak.Get())
				{
					if (AStructuralPowerGraphSubsystem* Graph =
						AStructuralPowerGraphSubsystem::GetOrCreate(WorldPtr))
					{
						Graph->EnqueuePlacement(
							LightPtr,
							EStructuralPlacementJobType::Outlet,
							/*bDefer=*/true);
					}
				}
			}
		}));
}

static void HandleLightBuildEffectFinished(AFGBuildableLightSource* Light)
{
	if (!IsValid(Light) || !Light->HasAuthority())
	{
		return;
	}

	UWorld* World = Light->GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	if (AStructuralPowerGraphSubsystem* Graph = AStructuralPowerGraphSubsystem::GetOrCreate(World))
	{
		FStructuralPowerTrace::LogHook(Light, TEXT("OnBuildEffectFinished"), TEXT("enqueue_light"), TEXT("defer"));
		Graph->EnqueuePlacement(Light, EStructuralPlacementJobType::Outlet, /*bDefer=*/true);
	}
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
		UE_LOG(LogStructuralPower, Log, TEXT("[HALSP] BlueprintConstruct deferred %d buildable(s)"), Enqueued);
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
		TEXT("[HALSP] LW hook AddFromBuildableInstanceData %s[%d]"),
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

	FStructuralPowerIdPresenterFactory::ResetForMapTravel();

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
	if (Phase == ELifecyclePhase::POST_INITIALIZATION)
	{
		FStructuralPowerModConfig::SyncRuntimeFromConfigManager(GetGameInstance());
#if !WITH_EDITOR
		UE_LOG(LogStructuralPower, Log,
			TEXT("StructuralPower v2.2.0 — lighting + Id panel (I)"
				" (groupLighting=%s propagation=%s gateSwitches=%s manualGroups=%s)"),
			FStructuralPowerModConfig::IsGroupLightingEnabled() ? TEXT("on") : TEXT("off"),
			FStructuralPowerModConfig::IsPropagationEnabled() ? TEXT("on") : TEXT("off"),
			FStructuralPowerModConfig::IsGatePowerSwitchesEnabled() ? TEXT("on") : TEXT("off"),
			FStructuralPowerModConfig::IsPowerSwitchManualGroupsEnabled() ? TEXT("on") : TEXT("off"));
#endif
	}

	if (Phase != ELifecyclePhase::INITIALIZATION)
	{
		Super::DispatchLifecycleEvent(Phase);
		return;
	}

#if WITH_EDITOR
	UE_LOG(LogStructuralPower, Log, TEXT("StructuralPower: skipping hooks in editor"));
#else
	FStructuralEquipmentBridgeRegistry::Get().Initialize();
	FStructuralPowerProcessorRegistry::Get().Initialize();
	FStructuralPowerIdInput::Register();

	SUBSCRIBE_METHOD_AFTER(
		UFGGameUI::RemoveInteractWidget,
		[](UFGGameUI* GameUI, UFGInteractWidget* /*Widget*/)
		{
			if (!IsValid(GameUI))
			{
				return;
			}

			if (AFGPlayerController* PC = Cast<AFGPlayerController>(GameUI->GetOwningPlayer()))
			{
				FStructuralPowerIdInput::RecoverAfterVanillaUiClosed(PC);
			}
		});

	SUBSCRIBE_METHOD_AFTER(
		AFGHUD::OpenInteractUI,
		[](AFGHUD* Hud, TSubclassOf<UFGInteractWidget> /*WidgetClass*/, UObject* /*InteractObject*/)
		{
			FStructuralPowerIdPresenterFactory::CloseActive();
			if (IsValid(Hud))
			{
				if (AFGPlayerController* PC = Cast<AFGPlayerController>(Hud->GetOwningPlayerController()))
				{
					FStructuralPowerIdPresenterFactory::ReleaseForVanillaInteract(PC);
				}
			}
		});

	SUBSCRIBE_METHOD_VIRTUAL(
		AFGBuildableControlPanelHost::BeginPlay,
		GetMutableDefault<AFGBuildableControlPanelHost>(),
		[](auto& /*Scope*/, AFGBuildableControlPanelHost* Host)
		{
			if (!IsValid(Host) || !Host->HasAuthority())
			{
				return;
			}

			if (Host->IsA<AFGBuildableLightsControlPanel>())
			{
				AStructuralPowerGraphSubsystem::StripPersistedEndpointModComponents(Host);
			}
		});

	SUBSCRIBE_METHOD_VIRTUAL_AFTER(
		AFGBuildableControlPanelHost::BeginPlay,
		GetMutableDefault<AFGBuildableControlPanelHost>(),
		[](AFGBuildableControlPanelHost* Host)
		{
			if (AFGBuildableLightsControlPanel* Panel = Cast<AFGBuildableLightsControlPanel>(Host))
			{
				HandlePanelBeginPlay(Panel);
			}
		});

	SUBSCRIBE_METHOD_VIRTUAL_AFTER(
		AFGBuildableCircuitBridge::OnCircuitsRebuilt,
		GetMutableDefault<AFGBuildableCircuitBridge>(),
		[](AFGBuildableCircuitBridge* Bridge)
		{
			if (!FStructuralPowerModConfig::IsGroupLightingEnabled())
			{
				return;
			}

			AFGBuildableLightsControlPanel* Panel = Cast<AFGBuildableLightsControlPanel>(Bridge);
			if (!IsValid(Panel) || !Panel->HasAuthority())
			{
				return;
			}

			UWorld* World = Panel->GetWorld();
			if (!IsValid(World))
			{
				return;
			}

			if (AStructuralPowerGraphSubsystem* Graph =
					AStructuralPowerGraphSubsystem::GetOrCreate(World))
			{
				if (Graph->ShouldDeferSwitchCircuitRefresh())
				{
					return;
				}

				if (Graph->IsBuildablePlacementPending(Panel))
				{
					return;
				}

				if (Graph->IsBulkLoadDrainActive())
				{
					return;
				}

				const FName Control = Graph->ResolveControl(Panel, EStructuralChannel::Light);
				if (Control.IsNone()
					|| Control == StructuralPowerConstants::ControlUnconfigured)
				{
					return;
				}

				if (FStructuralPowerTrace::IsEnabled())
				{
					FStructuralPowerTrace::LogHook(
						Panel,
						TEXT("OnCircuitsRebuilt"),
						TEXT("panel_subnet_sync"),
						TEXT("pre_apply"));
				}

				Graph->ProcessPanelWireDelta(Panel);
				FStructuralPanelControlledSync::ApplyKeyedSubnet(*Graph, Panel);

				if (FStructuralPowerTrace::IsEnabled())
				{
					Graph->LogPanelReconcileSummary(Panel);
				}
			}
		});

	SUBSCRIBE_METHOD_VIRTUAL_AFTER(
		AFGBuildableLightSource::BeginPlay,
		GetMutableDefault<AFGBuildableLightSource>(),
		[](AFGBuildableLightSource* Light)
		{
			HandleLightBeginPlay(Light);
		});

	SUBSCRIBE_METHOD_VIRTUAL_AFTER(
		AFGBuildableLightSource::OnBuildEffectFinished,
		GetMutableDefault<AFGBuildableLightSource>(),
		[](AFGBuildableLightSource* Light)
		{
			HandleLightBuildEffectFinished(Light);
		});

	SUBSCRIBE_METHOD_VIRTUAL(
		AFGBuildableCircuitSwitch::BeginPlay,
		GetMutableDefault<AFGBuildableCircuitSwitch>(),
		[](auto& /*Scope*/, AFGBuildableCircuitSwitch* Switch)
		{
			if (!IsValid(Switch) || !Switch->HasAuthority())
			{
				return;
			}

			AStructuralPowerGraphSubsystem::StripPersistedEndpointModComponents(Switch);
		});

	SUBSCRIBE_METHOD_VIRTUAL_AFTER(
		AFGBuildableCircuitSwitch::BeginPlay,
		GetMutableDefault<AFGBuildableCircuitSwitch>(),
		[](AFGBuildableCircuitSwitch* Switch)
		{
			HandleSwitchBeginPlay(Switch);
		});

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
