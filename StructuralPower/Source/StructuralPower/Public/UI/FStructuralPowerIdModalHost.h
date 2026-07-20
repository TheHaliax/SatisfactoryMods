// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class AFGPlayerController;
class UStructuralPowerIdConfigWidget;

class STRUCTURALPOWER_API FStructuralPowerIdModalHost {
 public:
  static void NormalizeModalState(UStructuralPowerIdConfigWidget* Widget);

  static void DetachFromViewport(UStructuralPowerIdConfigWidget* Widget);

  static void ApplyModalInputMode(UStructuralPowerIdConfigWidget* Widget);

  static void ScheduleModalInputModeRefresh(UStructuralPowerIdConfigWidget* Widget);

  static void ForceReleaseAllModalState(AFGPlayerController* PC, bool bRestoreGameInputMode = true);

  static bool OwnsPlayerInput();

  static void ClearInputOwnership();

  static void RegisterEscapeInputProcessor();

  static void UnregisterEscapeInputProcessor();

  static void DismissBlockingGameMessages(AFGPlayerController* PC);

  static void OnPanelOpened(UStructuralPowerIdConfigWidget* Widget, AFGPlayerController* PC);

  static void OnPanelClosed(UStructuralPowerIdConfigWidget* Widget, AFGPlayerController* PC);

 private:
  static TSharedPtr<class IInputProcessor> EscapeInputProcessor;
  static bool bOwnsPlayerInput;
};
