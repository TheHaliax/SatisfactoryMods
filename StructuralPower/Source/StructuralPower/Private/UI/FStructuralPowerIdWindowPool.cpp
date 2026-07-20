// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "UI/FStructuralPowerIdWindowPool.h"

#include "FGPlayerController.h"
#include "StructuralPowerLog.h"
#include "UI/FStructuralPowerIdModalHost.h"
#include "UI/UStructuralPowerIdConfigWidget.h"

TWeakObjectPtr<UStructuralPowerIdConfigWidget> FStructuralPowerIdWindowPool::ActiveInstance;
TObjectPtr<UStructuralPowerIdConfigWidget> FStructuralPowerIdWindowPool::CachedWindow;

UStructuralPowerIdConfigWidget* FStructuralPowerIdWindowPool::GetActiveWidget() {
  return ActiveInstance.Get();
}

UStructuralPowerIdConfigWidget* FStructuralPowerIdWindowPool::GetPooledWindow() {
  return CachedWindow.Get();
}

UStructuralPowerIdConfigWidget* FStructuralPowerIdWindowPool::GetOrCreateWindow(
    AFGPlayerController* PC) {
  if (!IsValid(PC)) {
    return nullptr;
  }

  if (UStructuralPowerIdConfigWidget* Existing = CachedWindow.Get()) {
    if (IsValid(Existing)) {
      return Existing;
    }

    CachedWindow = nullptr;
  }

  UStructuralPowerIdConfigWidget* Window = CreateWidget<UStructuralPowerIdConfigWidget>(
      PC, UStructuralPowerIdConfigWidget::StaticClass());
  if (!IsValid(Window)) {
    return nullptr;
  }

  CachedWindow = Window;
  UE_LOG(LogStructuralPower, Log, TEXT("[HALSP] Id panel window created"));
  return Window;
}

void FStructuralPowerIdWindowPool::SetActiveWidget(UStructuralPowerIdConfigWidget* Widget) {
  if (Widget == nullptr || !IsValid(Widget)) {
    ActiveInstance.Reset();
    return;
  }

  ActiveInstance = Widget;
}

void FStructuralPowerIdWindowPool::ResetForMapLoad() {
  ActiveInstance.Reset();
  CachedWindow = nullptr;
  FStructuralPowerIdModalHost::UnregisterEscapeInputProcessor();
  FStructuralPowerIdModalHost::ClearInputOwnership();
}

void FStructuralPowerIdWindowPool::CloseActivePanel() {
  if (UStructuralPowerIdConfigWidget* Widget = ActiveInstance.Get()) {
    Widget->ClosePanel();
    return;
  }

  if (UStructuralPowerIdConfigWidget* Window = CachedWindow.Get()) {
    FStructuralPowerIdModalHost::DetachFromViewport(Window);
    if (AFGPlayerController* PC = Window->GetOwningPlayer<AFGPlayerController>()) {
      FStructuralPowerIdModalHost::ForceReleaseAllModalState(PC);
    }
  }
}

void FStructuralPowerIdWindowPool::ReleaseForVanillaInteract(AFGPlayerController* PC) {
  if (!IsValid(PC)) {
    return;
  }

  const bool bHadActivePanel = ActiveInstance.IsValid();
  if (UStructuralPowerIdConfigWidget* Widget = ActiveInstance.Get()) {
    FStructuralPowerIdModalHost::DetachFromViewport(Widget);
    Widget->bModalActive = false;
    if (ActiveInstance.Get() == Widget) {
      SetActiveWidget(nullptr);
    }
  } else if (UStructuralPowerIdConfigWidget* Window = CachedWindow.Get()) {
    if (IsValid(Window) && Window->IsPanelVisible()) {
      FStructuralPowerIdModalHost::DetachFromViewport(Window);
      Window->bModalActive = false;
    }
  }

  if (!FStructuralPowerIdModalHost::OwnsPlayerInput() && !bHadActivePanel) {
    return;
  }

  FStructuralPowerIdModalHost::ForceReleaseAllModalState(PC, /*bRestoreGameInputMode=*/false);
}

void FStructuralPowerIdWindowPool::NotifyWindowDestroyed(UStructuralPowerIdConfigWidget* Widget) {
  if (!IsValid(Widget)) {
    return;
  }

  if (ActiveInstance.Get() == Widget) {
    SetActiveWidget(nullptr);
  }

  if (CachedWindow.Get() == Widget) {
    CachedWindow = nullptr;
  }
}
