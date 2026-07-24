// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Input/FRemoteStandbyInput.h"

#include "Buildables/FGBuildableFactory.h"
#include "Engine/GameViewportClient.h"
#include "FGCharacterPlayer.h"
#include "FGPlayerController.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/CoreMisc.h"
#include "Network/URSStandbyRCO.h"
#include "RemoteStandbyLog.h"
#include "UI/FGGameUI.h"
#include "View/FRSViewTarget.h"

FDelegateHandle FRemoteStandbyInput::InputInitHandle;
TSharedPtr<IInputProcessor> FRemoteStandbyInput::HotkeyInputProcessor;

namespace {
AFGPlayerController* ResolveLocalPlayerController() {
  if (!GEngine || !GEngine->GameViewport) {
    return nullptr;
  }

  UWorld* World = GEngine->GameViewport->GetWorld();
  if (!IsValid(World) || !World->IsGameWorld()) {
    return nullptr;
  }

  AFGPlayerController* PC = Cast<AFGPlayerController>(World->GetFirstPlayerController());
  if (!IsValid(PC) || !PC->IsLocalController()) {
    return nullptr;
  }

  return PC;
}

class FRemoteStandbyHotkeyProcessor : public IInputProcessor {
 public:
  virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp,
                                  const FKeyEvent& InKeyEvent) override {
    if (InKeyEvent.GetKey() != EKeys::Z) {
      return false;
    }

    AFGPlayerController* PC = ResolveLocalPlayerController();
    if (!IsValid(PC)) {
      return false;
    }

    if (UFGGameUI* GameUI = PC->GetGameUI()) {
      if (IsValid(GameUI) && GameUI->HasActiveInteractWidget()) {
        return false;
      }
    }

    FRemoteStandbyInput::TryToggleLookedAtFactory(PC);
    return true;
  }

  virtual void Tick(const float DeltaTime, FSlateApplication& /*SlateApp*/,
                    TSharedRef<ICursor> /*Cursor*/) override {
  }
};
} // namespace

void FRemoteStandbyInput::EnsureHotkeyProcessorRegistered() {
  if (HotkeyInputProcessor.IsValid()) {
    return;
  }

  if (IsRunningDedicatedServer()) {
    return;
  }

  if (!GEngine || !GEngine->GameViewport) {
    return;
  }

  if (!FSlateApplication::IsInitialized()) {
    return;
  }

  HotkeyInputProcessor = MakeShared<FRemoteStandbyHotkeyProcessor>();
  FSlateApplication::Get().RegisterInputPreProcessor(HotkeyInputProcessor, 10);
  UE_LOG(LogRemoteStandby, Log, TEXT("%s Z hotkey processor registered"), REMOTESTANDBY_LOG_PREFIX);
}

void FRemoteStandbyInput::UnregisterHotkeyProcessor() {
  if (!HotkeyInputProcessor.IsValid()) {
    return;
  }

  if (FSlateApplication::IsInitialized()) {
    FSlateApplication::Get().UnregisterInputPreProcessor(HotkeyInputProcessor);
  }

  HotkeyInputProcessor.Reset();
}

void FRemoteStandbyInput::EnsureInputReady(AFGPlayerController* PlayerController) {
  if (!IsValid(PlayerController) || !PlayerController->IsLocalController()) {
    return;
  }

  if (IsRunningDedicatedServer()) {
    return;
  }

  UWorld* World = PlayerController->GetWorld();
  if (!IsValid(World) || World->bIsTearingDown) {
    return;
  }

  EnsureHotkeyProcessorRegistered();
}

void FRemoteStandbyInput::Register() {
  if (InputInitHandle.IsValid()) {
    return;
  }

  if (IsRunningDedicatedServer()) {
    return;
  }

  InputInitHandle = AFGCharacterPlayer::OnPlayerInputInitialized.AddStatic(
      &FRemoteStandbyInput::OnPlayerInputInitialized);
}

void FRemoteStandbyInput::Unregister() {
  UnregisterHotkeyProcessor();

  if (InputInitHandle.IsValid()) {
    AFGCharacterPlayer::OnPlayerInputInitialized.Remove(InputInitHandle);
    InputInitHandle.Reset();
  }
}

void FRemoteStandbyInput::OnPlayerInputInitialized(AFGCharacterPlayer* Player,
                                                   UInputComponent* /*InputComponent*/) {
  if (!IsValid(Player)) {
    return;
  }

  AFGPlayerController* PlayerController = Cast<AFGPlayerController>(Player->GetController());
  if (!IsValid(PlayerController) || !PlayerController->IsLocalController()) {
    return;
  }

  EnsureInputReady(PlayerController);
}

void FRemoteStandbyInput::TryToggleLookedAtFactory(AFGPlayerController* PlayerController) {
  if (!IsValid(PlayerController) || !PlayerController->IsLocalController()) {
    return;
  }

  AFGBuildableFactory* Factory = nullptr;
  if (!FRSViewTarget::PickFactory(PlayerController, Factory) || !IsValid(Factory)) {
    UE_LOG(LogRemoteStandby, Log, TEXT("%s Z — no factory under view"), REMOTESTANDBY_LOG_PREFIX);
    return;
  }

  URSStandbyRCO* Rco = PlayerController->GetRemoteCallObjectOfClass<URSStandbyRCO>();
  if (!Rco) {
    UE_LOG(LogRemoteStandby, Warning, TEXT("%s Z — missing URSStandbyRCO"),
           REMOTESTANDBY_LOG_PREFIX);
    return;
  }

  Rco->Server_ToggleStandby(Factory);
}
