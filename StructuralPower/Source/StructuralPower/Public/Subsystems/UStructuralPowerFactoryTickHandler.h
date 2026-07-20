// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "FGBuildableSubsystem.h"
#include "UStructuralPowerFactoryTickHandler.generated.h"

class AFGBuildableSubsystem;

UCLASS()
class STRUCTURALPOWER_API UStructuralPowerFactoryTickHandler
    : public UObject,
      public IFGFactoryTickHandlerInterface {
  GENERATED_BODY()

 public:
  static void RegisterForWorld(UWorld* World);
  static void UnregisterForWorld(UWorld* World);
  static void UnregisterGlobalDelegates();

  virtual void PostFactoryTick(AFGBuildableSubsystem* Subsystem, float DeltaTime) override;

 private:
  static void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);

  static TMap<TWeakObjectPtr<UWorld>, TObjectPtr<UStructuralPowerFactoryTickHandler>> Handlers;
  static FDelegateHandle WorldCleanupHandle;
};
