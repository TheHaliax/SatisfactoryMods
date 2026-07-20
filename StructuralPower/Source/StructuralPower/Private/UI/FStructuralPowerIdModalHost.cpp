// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "UI/FStructuralPowerIdModalHost.h"

#include "FGHUD.h"
#include "FGPlayerController.h"
#include "FGPlayerControllerBase.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "Input/Events.h"
#include "Input/FStructuralPowerIdInput.h"
#include "StructuralPowerLog.h"
#include "TimerManager.h"
#include "UI/FGGameUI.h"
#include "UI/FStructuralPowerIdWindowPool.h"
#include "UI/Message/FGAudioMessage.h"
#include "UI/UStructuralPowerIdConfigWidget.h"

namespace {
class FStructuralPowerIdEscapeProcessor : public IInputProcessor {
 public:
  virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp,
                                  const FKeyEvent& InKeyEvent) override {
    if (InKeyEvent.GetKey() != EKeys::Escape) {
      return false;
    }

    UStructuralPowerIdConfigWidget* Active = FStructuralPowerIdWindowPool::GetActiveWidget();
    if (!IsValid(Active) || !Active->IsPanelVisible()) {
      return false;
    }

    Active->ClosePanel();
    return true;
  }

  virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp,
                    TSharedRef<ICursor> Cursor) override {
  }
};
}

TSharedPtr<IInputProcessor> FStructuralPowerIdModalHost::EscapeInputProcessor;
bool FStructuralPowerIdModalHost::bOwnsPlayerInput = false;

bool FStructuralPowerIdModalHost::OwnsPlayerInput() {
  return bOwnsPlayerInput;
}

void FStructuralPowerIdModalHost::ClearInputOwnership() {
  bOwnsPlayerInput = false;
}

void FStructuralPowerIdModalHost::NormalizeModalState(UStructuralPowerIdConfigWidget* Widget) {
  if (!IsValid(Widget)) {
    return;
  }

  if (Widget->bModalActive && !Widget->IsPanelShown()) {
    Widget->bModalActive = false;
    if (FStructuralPowerIdWindowPool::GetActiveWidget() == Widget) {
      FStructuralPowerIdWindowPool::SetActiveWidget(nullptr);
    }

    if (AFGPlayerController* PC = Widget->GetOwningPlayer<AFGPlayerController>()) {
      UnregisterEscapeInputProcessor();
      ForceReleaseAllModalState(PC);
    }

    UE_LOG(LogStructuralPower, Warning,
           TEXT("[HALSP] Id panel recovered stale modal (active=1 shown=0)"));
  } else if (!Widget->bModalActive && Widget->IsPanelShown()) {
    Widget->SetVisibility(ESlateVisibility::Hidden);
    Widget->SetIsEnabled(false);
  }
}

void FStructuralPowerIdModalHost::DetachFromViewport(UStructuralPowerIdConfigWidget* Widget) {
  if (!IsValid(Widget)) {
    return;
  }

  UnregisterEscapeInputProcessor();
  Widget->SetVisibility(ESlateVisibility::Collapsed);
  Widget->SetIsEnabled(false);

  if (Widget->IsInViewport()) {
    Widget->RemoveFromParent();
  }
}

void FStructuralPowerIdModalHost::ApplyModalInputMode(UStructuralPowerIdConfigWidget* Widget) {
  if (!IsValid(Widget) || !Widget->bModalActive) {
    return;
  }

  AFGPlayerController* PC = Widget->GetOwningPlayer<AFGPlayerController>();
  if (!IsValid(PC)) {
    return;
  }

  if (!bOwnsPlayerInput) {
    if (APawn* Pawn = PC->GetPawn()) {
      Pawn->DisableInput(PC);
    }

    PC->SetIgnoreLookInput(true);
    PC->SetIgnoreMoveInput(true);
  }

  PC->bShowMouseCursor = true;
  PC->bEnableClickEvents = true;
  PC->bEnableMouseOverEvents = true;

  FInputModeUIOnly InputMode;
  InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);

  UWidget* FocusWidget = IsValid(Widget->CloseButton)
                             ? static_cast<UWidget*>(Widget->CloseButton.Get())
                             : static_cast<UWidget*>(Widget);
  TSharedPtr<SWidget> SlateWidget = FocusWidget->GetCachedWidget();
  if (!SlateWidget.IsValid()) {
    SlateWidget = Widget->GetCachedWidget();
  }

  if (SlateWidget.IsValid()) {
    InputMode.SetWidgetToFocus(SlateWidget);
    if (FSlateApplication::IsInitialized()) {
      FSlateApplication::Get().SetKeyboardFocus(SlateWidget, EFocusCause::SetDirectly);
      const int32 UserIndex = PC->GetLocalPlayer() ? PC->GetLocalPlayer()->GetControllerId() : 0;
      FSlateApplication::Get().SetUserFocus(UserIndex, SlateWidget, EFocusCause::SetDirectly);
    }
  }

  PC->SetInputMode(InputMode);

  if (AFGHUD* HUD = PC->GetHUD<AFGHUD>()) {
    HUD->SetForceHideCrossHair(true);
    HUD->SetShowCrossHair(false);
  }

  bOwnsPlayerInput = true;

  UE_LOG(LogStructuralPower, Verbose,
         TEXT("[HALSP] Id panel input=UIOnly cursor=%d slateFocus=%d inViewport=%d"),
         PC->bShowMouseCursor ? 1 : 0, SlateWidget.IsValid() ? 1 : 0,
         Widget->IsInViewport() ? 1 : 0);
}

void FStructuralPowerIdModalHost::ScheduleModalInputModeRefresh(
    UStructuralPowerIdConfigWidget* Widget) {
  if (!IsValid(Widget)) {
    return;
  }

  if (UWorld* World = Widget->GetWorld()) {
    TWeakObjectPtr<UStructuralPowerIdConfigWidget> WeakWidget(Widget);
    World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakWidget]() {
      if (UStructuralPowerIdConfigWidget* Resolved = WeakWidget.Get()) {
        ApplyModalInputMode(Resolved);
      }
    }));
  }
}

void FStructuralPowerIdModalHost::ForceReleaseAllModalState(AFGPlayerController* PC,
                                                            bool bRestoreGameInputMode) {
  if (!IsValid(PC)) {
    return;
  }

  if (!bOwnsPlayerInput) {
    return;
  }

  UnregisterEscapeInputProcessor();

  PC->ResetIgnoreMoveInput();
  PC->ResetIgnoreLookInput();

  if (APawn* Pawn = PC->GetPawn()) {
    Pawn->EnableInput(PC);
  }

  bOwnsPlayerInput = false;

  if (!bRestoreGameInputMode) {
    UE_LOG(LogStructuralPower, Verbose,
           TEXT("[HALSP] Id panel released capture (vanilla interact owns input)"));
    return;
  }

  if (UFGGameUI* GameUI = PC->GetGameUI()) {
    if (IsValid(GameUI) && GameUI->HasActiveInteractWidget()) {
      UE_LOG(LogStructuralPower, Verbose,
             TEXT("[HALSP] Id panel skip GameOnly — vanilla interact active"));
      return;
    }
  }

  PC->bShowMouseCursor = false;
  PC->bEnableClickEvents = false;
  PC->bEnableMouseOverEvents = false;

  FInputModeGameOnly InputMode;
  PC->SetInputMode(InputMode);
  PC->FlushPressedKeys();

  if (AFGHUD* HUD = PC->GetHUD<AFGHUD>()) {
    HUD->SetForceHideCrossHair(false);
    HUD->SetShowCrossHair(true);
  }

  UE_LOG(LogStructuralPower, Log, TEXT("[HALSP] Id panel force-released modal state"));
}

void FStructuralPowerIdModalHost::RegisterEscapeInputProcessor() {
  if (EscapeInputProcessor.IsValid()) {
    return;
  }

  EscapeInputProcessor = MakeShared<FStructuralPowerIdEscapeProcessor>();
  if (FSlateApplication::IsInitialized()) {
    FSlateApplication::Get().RegisterInputPreProcessor(EscapeInputProcessor, 0);
  }
}

void FStructuralPowerIdModalHost::UnregisterEscapeInputProcessor() {
  if (!EscapeInputProcessor.IsValid()) {
    return;
  }

  if (FSlateApplication::IsInitialized()) {
    FSlateApplication::Get().UnregisterInputPreProcessor(EscapeInputProcessor);
  }

  EscapeInputProcessor.Reset();
}

void FStructuralPowerIdModalHost::DismissBlockingGameMessages(AFGPlayerController* PC) {
  if (!IsValid(PC)) {
    return;
  }

  UFGGameUI* GameUI = PC->GetGameUI();
  if (!IsValid(GameUI)) {
    return;
  }

  if (UFGAudioMessage* ActiveMessage = GameUI->GetActiveAudioMessage()) {
    ActiveMessage->CancelPlayback();
    GameUI->AudioMessageFinishedPlayback();
    UE_LOG(LogStructuralPower, Log, TEXT("[HALSP] Id panel dismissed active audio message"));
  }
}

void FStructuralPowerIdModalHost::OnPanelOpened(UStructuralPowerIdConfigWidget* Widget,
                                                AFGPlayerController* PC) {
  if (!IsValid(Widget) || !IsValid(PC)) {
    return;
  }

  FStructuralPowerIdWindowPool::SetActiveWidget(Widget);
  Widget->bModalActive = true;
  RegisterEscapeInputProcessor();
  ApplyModalInputMode(Widget);
  ScheduleModalInputModeRefresh(Widget);
  DismissBlockingGameMessages(PC);
  FStructuralPowerIdInput::NotifyPanelOpened(PC);
}

void FStructuralPowerIdModalHost::OnPanelClosed(UStructuralPowerIdConfigWidget* Widget,
                                                AFGPlayerController* PC) {
  if (IsValid(PC)) {
    FStructuralPowerIdInput::NotifyPanelClosed(PC);
  }
}
