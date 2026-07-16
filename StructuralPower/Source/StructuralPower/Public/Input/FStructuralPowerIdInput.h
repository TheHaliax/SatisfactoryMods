// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class AFGCharacterPlayer;
class AFGPlayerController;
class UInputComponent;

class STRUCTURALPOWER_API FStructuralPowerIdInput {
 public:
  static void Register();
  static void Unregister();
  static void OpenIdPanelForController(AFGPlayerController* PlayerController);
  static void NotifyPanelOpened(AFGPlayerController* PlayerController);
  static void NotifyPanelClosed(AFGPlayerController* PlayerController);
  static void EnsureGameInputUnblocked(AFGPlayerController* PlayerController);
  static void EnsureInputReady(AFGPlayerController* PlayerController);
  static void RecoverAfterVanillaUiClosed(AFGPlayerController* PlayerController);

 private:
  static void EnsureHotkeyProcessorRegistered();
  static void UnregisterHotkeyProcessor();
  static void OnPlayerInputInitialized(AFGCharacterPlayer* Player, UInputComponent* InputComponent);

  static FDelegateHandle InputInitHandle;
  static TSharedPtr<class IInputProcessor> HotkeyInputProcessor;
};
