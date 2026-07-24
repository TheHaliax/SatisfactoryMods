// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class AFGCharacterPlayer;
class AFGPlayerController;
class UInputComponent;

class REMOTESTANDBY_API FRemoteStandbyInput {
 public:
  static void Register();
  static void Unregister();
  static void EnsureInputReady(AFGPlayerController* PlayerController);
  static void TryToggleLookedAtFactory(AFGPlayerController* PlayerController);

 private:
  static void EnsureHotkeyProcessorRegistered();
  static void UnregisterHotkeyProcessor();
  static void OnPlayerInputInitialized(AFGCharacterPlayer* Player, UInputComponent* InputComponent);

  static FDelegateHandle InputInitHandle;
  static TSharedPtr<class IInputProcessor> HotkeyInputProcessor;
};
