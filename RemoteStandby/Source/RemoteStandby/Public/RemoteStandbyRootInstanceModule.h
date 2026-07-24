// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Module/GameInstanceModule.h"
#include "RemoteStandbyRootInstanceModule.generated.h"

UCLASS()
class REMOTESTANDBY_API URemoteStandbyRootInstanceModule : public UGameInstanceModule {
  GENERATED_BODY()

 public:
  URemoteStandbyRootInstanceModule();

  virtual void DispatchLifecycleEvent(ELifecyclePhase Phase) override;

  static void UnregisterGlobalDelegates();

  static URemoteStandbyRootInstanceModule* Find(UWorld* World);

 private:
  static FDelegateHandle PostLoadMapHandle;
};
