// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "RemoteStandbyRootInstanceModule.h"

#include "Engine/GameInstance.h"
#include "Input/FRemoteStandbyInput.h"
#include "Module/GameInstanceModuleManager.h"
#include "Network/URSStandbyRCO.h"
#include "RemoteStandbyLog.h"

FDelegateHandle URemoteStandbyRootInstanceModule::PostLoadMapHandle;

URemoteStandbyRootInstanceModule::URemoteStandbyRootInstanceModule() {
  bRootModule = true;
  RemoteCallObjects.Add(URSStandbyRCO::StaticClass());
}

URemoteStandbyRootInstanceModule* URemoteStandbyRootInstanceModule::Find(UWorld* World) {
  if (!IsValid(World)) {
    return nullptr;
  }
  UGameInstance* GI = World->GetGameInstance();
  if (!GI) {
    return nullptr;
  }
  UGameInstanceModuleManager* Mgr = GI->GetSubsystem<UGameInstanceModuleManager>();
  if (!Mgr) {
    return nullptr;
  }
  return Cast<URemoteStandbyRootInstanceModule>(Mgr->FindModule(FName(TEXT("RemoteStandby"))));
}

void URemoteStandbyRootInstanceModule::UnregisterGlobalDelegates() {
  if (PostLoadMapHandle.IsValid()) {
    FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
    PostLoadMapHandle.Reset();
  }
  FRemoteStandbyInput::Unregister();
}

void URemoteStandbyRootInstanceModule::DispatchLifecycleEvent(ELifecyclePhase Phase) {
  if (Phase == ELifecyclePhase::INITIALIZATION) {
#if WITH_EDITOR
    UE_LOG(LogRemoteStandby, Log, TEXT("%s skip input hooks in editor"), REMOTESTANDBY_LOG_PREFIX);
#else
    FRemoteStandbyInput::Register();
#endif
  }

  if (Phase == ELifecyclePhase::POST_INITIALIZATION) {
    UE_LOG(LogRemoteStandby, Display, TEXT("%s RemoteStandby v0.1.0 — Z look-at standby toggle"),
           REMOTESTANDBY_LOG_PREFIX);
  }
  Super::DispatchLifecycleEvent(Phase);
}
