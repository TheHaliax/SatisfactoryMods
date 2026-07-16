// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class AFGPlayerController;

class PIPELINECOLOR_API FPCBangCommands {
 public:
  static void RegisterChatHook();
  static void Execute(AFGPlayerController* PlayerController, const FString& CommandLine);
};
