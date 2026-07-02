// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Module/GameInstanceModule.h"
#include "StructuralPowerRootInstanceModule.generated.h"

class AFGBuildable;
class AFGLightweightBuildableSubsystem;

UCLASS()
class STRUCTURALPOWER_API UStructuralPowerRootInstanceModule : public UGameInstanceModule
{
	GENERATED_BODY()

public:
	UStructuralPowerRootInstanceModule();

	virtual void DispatchLifecycleEvent(ELifecyclePhase Phase) override;

	static void UnregisterGlobalDelegates();

private:
	static bool TryEnqueueBuildable(AFGBuildable* Buildable, const TCHAR* HookName, const TCHAR* SourceTag);
	static void HandleBuildableBuilt(AFGBuildable* Buildable);
	static void HandleBuildablesConstructed(const TArray<AActor*>& Children);
	static void HandleBuildableRemoved(AFGBuildable* Buildable);
	static void HandlePowerConnectionChanged(
		AFGBuildablePowerPole* Pole,
		UFGCircuitConnectionComponent* Connection);
	static void HandleLightweightMemberAdded(
		AFGLightweightBuildableSubsystem* Subsystem,
		TSubclassOf<AFGBuildable> BuildableClass,
		int32 InstanceIndex);
	static void HandleLightweightMemberRemoved(
		AFGLightweightBuildableSubsystem* Subsystem,
		TSubclassOf<AFGBuildable> BuildableClass,
		int32 InstanceIndex);
	static void HandlePostLoadMap(UWorld* World);

	static FDelegateHandle PostLoadMapHandle;
};
