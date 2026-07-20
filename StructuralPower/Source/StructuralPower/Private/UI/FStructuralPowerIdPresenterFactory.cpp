// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "UI/FStructuralPowerIdPresenterFactory.h"

#include "UI/FStructuralPowerUmgIdPresenter.h"

TUniquePtr<IStructuralPowerIdPresenter> FStructuralPowerIdPresenterFactory::ActivePresenter;

void FStructuralPowerIdPresenterFactory::EnsureDefaultImplementation() {
  if (!ActivePresenter.IsValid()) {
    ActivePresenter = MakeUnique<FStructuralPowerUmgIdPresenter>();
  }
}

IStructuralPowerIdPresenter& FStructuralPowerIdPresenterFactory::Get() {
  EnsureDefaultImplementation();
  return *ActivePresenter;
}

void FStructuralPowerIdPresenterFactory::ResetForMapTravel() {
  Get().ResetForMapTravel();
}

void FStructuralPowerIdPresenterFactory::CloseActive() {
  Get().Close();
}

void FStructuralPowerIdPresenterFactory::ForceReleaseModalState(
    AFGPlayerController* PlayerController, const bool bRestoreGameInputMode) {
  Get().ForceReleaseModalState(PlayerController, bRestoreGameInputMode);
}

void FStructuralPowerIdPresenterFactory::ReleaseForVanillaInteract(
    AFGPlayerController* PlayerController) {
  Get().ReleaseForVanillaInteract(PlayerController);
}

void FStructuralPowerIdPresenterFactory::PrepareWindowForController(
    AFGPlayerController* PlayerController) {
  Get().PrepareForController(PlayerController);
}

void FStructuralPowerIdPresenterFactory::NormalizeModalState() {
  Get().NormalizeModalState();
}

AFGBuildable* FStructuralPowerIdPresenterFactory::GetOpenTarget() {
  return Get().GetOpenTarget();
}

bool FStructuralPowerIdPresenterFactory::IsAnyPanelVisible() {
  return Get().IsAnyPanelVisible();
}

bool FStructuralPowerIdPresenterFactory::IsTextFieldFocused() {
  return Get().IsTextFieldFocused();
}
