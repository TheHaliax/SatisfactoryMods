// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "RemoteStandbyModule.h"

#include "RemoteStandbyRootInstanceModule.h"

void FRemoteStandbyModule::StartupModule() {
}

void FRemoteStandbyModule::ShutdownModule() {
  URemoteStandbyRootInstanceModule::UnregisterGlobalDelegates();
}

IMPLEMENT_MODULE(FRemoteStandbyModule, RemoteStandby)
