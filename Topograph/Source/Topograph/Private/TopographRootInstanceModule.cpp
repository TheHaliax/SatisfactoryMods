// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TopographRootInstanceModule.h"

#include "Engine/World.h"
#include "Scan/UTopographWorldSubsystem.h"
#include "TopographLog.h"

FDelegateHandle UTopographRootInstanceModule::PostLoadMapHandle;

UTopographRootInstanceModule::UTopographRootInstanceModule() {
  bRootModule = true;
}

void UTopographRootInstanceModule::UnregisterGlobalDelegates() {
  if (PostLoadMapHandle.IsValid()) {
    FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
    PostLoadMapHandle.Reset();
  }
}

void UTopographRootInstanceModule::HandlePostLoadMap(UWorld* World) {
  if (!World) {
    return;
  }
  UE_LOG(LogTopograph, Log, TEXT("PostLoadMap netmode=%d"),
         static_cast<int32>(World->GetNetMode()));
}

void UTopographRootInstanceModule::DispatchLifecycleEvent(
    ELifecyclePhase Phase) {
  Super::DispatchLifecycleEvent(Phase);
  if (Phase == ELifecyclePhase::INITIALIZATION) {
    if (!PostLoadMapHandle.IsValid()) {
      PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddStatic(
          &UTopographRootInstanceModule::HandlePostLoadMap);
    }
    UE_LOG(LogTopograph, Log, TEXT("Topograph v0.1 root module initialized"));
  }
}
