// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TopographModule.h"

#include "HAL/IConsoleManager.h"
#include "Scan/UTopographWorldSubsystem.h"
#include "TopographLog.h"
#include "TopographRootInstanceModule.h"

DEFINE_LOG_CATEGORY(LogTopograph);

#define LOCTEXT_NAMESPACE "FTopographModule"

static FAutoConsoleCommandWithWorld GTopographStartCmd(
    TEXT("Topograph.Start"), TEXT("Start Topograph world bake"),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World) {
      if (!World) {
        return;
      }
      if (UTopographWorldSubsystem* Sys =
              World->GetSubsystem<UTopographWorldSubsystem>()) {
        Sys->StartScan();
      }
    }));

static FAutoConsoleCommandWithWorld GTopographStopCmd(
    TEXT("Topograph.Stop"), TEXT("Stop Topograph world bake"),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World) {
      if (!World) {
        return;
      }
      if (UTopographWorldSubsystem* Sys =
              World->GetSubsystem<UTopographWorldSubsystem>()) {
        Sys->StopScan();
      }
    }));

static FAutoConsoleCommandWithWorld GTopographStatusCmd(
    TEXT("Topograph.Status"), TEXT("Print Topograph bake status"),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World) {
      if (!World) {
        return;
      }
      if (UTopographWorldSubsystem* Sys =
              World->GetSubsystem<UTopographWorldSubsystem>()) {
        UE_LOG(LogTopograph, Display, TEXT("%s"), *Sys->GetStatusString());
      }
    }));

void FTopographModule::StartupModule() {}

void FTopographModule::ShutdownModule() {
  UTopographRootInstanceModule::UnregisterGlobalDelegates();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FTopographModule, Topograph)
