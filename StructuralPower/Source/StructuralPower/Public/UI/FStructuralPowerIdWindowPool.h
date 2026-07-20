// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class AFGPlayerController;
class UStructuralPowerIdConfigWidget;

class STRUCTURALPOWER_API FStructuralPowerIdWindowPool {
 public:
  static UStructuralPowerIdConfigWidget* GetActiveWidget();

  static UStructuralPowerIdConfigWidget* GetPooledWindow();

  static UStructuralPowerIdConfigWidget* GetOrCreateWindow(AFGPlayerController* PC);

  static void SetActiveWidget(UStructuralPowerIdConfigWidget* Widget);

  static void ResetForMapLoad();

  static void CloseActivePanel();

  static void ReleaseForVanillaInteract(AFGPlayerController* PC);

  static void NotifyWindowDestroyed(UStructuralPowerIdConfigWidget* Widget);

 private:
  static TWeakObjectPtr<UStructuralPowerIdConfigWidget> ActiveInstance;
  static TObjectPtr<UStructuralPowerIdConfigWidget> CachedWindow;
};
