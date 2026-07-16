// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Module/GameInstanceModule.h"
#include "TopographRootInstanceModule.generated.h"

class UWorld;

UCLASS()
class TOPOGRAPH_API UTopographRootInstanceModule : public UGameInstanceModule {
  GENERATED_BODY()

 public:
  UTopographRootInstanceModule();

  virtual void DispatchLifecycleEvent(ELifecyclePhase Phase) override;

  static void UnregisterGlobalDelegates();

 private:
  static void HandlePostLoadMap(UWorld* World);

  static FDelegateHandle PostLoadMapHandle;
};
