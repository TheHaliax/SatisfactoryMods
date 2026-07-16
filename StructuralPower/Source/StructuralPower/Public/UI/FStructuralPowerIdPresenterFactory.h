// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "UI/IStructuralPowerIdPresenter.h"

class AFGPlayerController;

class STRUCTURALPOWER_API FStructuralPowerIdPresenterFactory {
 public:
  static IStructuralPowerIdPresenter& Get();
  static void SetImplementation(TUniquePtr<IStructuralPowerIdPresenter> Impl);

  static void ResetForMapTravel();
  static void CloseActive();
  static void ForceReleaseModalState(AFGPlayerController* PlayerController,
                                     bool bRestoreGameInputMode = true);
  static void ReleaseForVanillaInteract(AFGPlayerController* PlayerController);
  static void PrepareWindowForController(AFGPlayerController* PlayerController);
  static void NormalizeModalState();
  static AFGBuildable* GetOpenTarget();
  static bool IsAnyPanelVisible();
  static bool IsTextFieldFocused();

 private:
  static void EnsureDefaultImplementation();
  static TUniquePtr<IStructuralPowerIdPresenter> ActivePresenter;
};
