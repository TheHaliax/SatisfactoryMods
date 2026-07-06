// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Input/FStructuralPowerIdInput.h"

#include "Buildables/FGBuildable.h"
#include "Engine/GameViewportClient.h"
#include "FGCharacterPlayer.h"
#include "FGPlayerController.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "StructuralPowerLog.h"
#include "UI/FGGameUI.h"
#include "UI/FStructuralIdConfigTarget.h"
#include "UI/UStructuralPowerIdConfigWidget.h"

FDelegateHandle FStructuralPowerIdInput::InputInitHandle;
TSharedPtr<IInputProcessor> FStructuralPowerIdInput::HotkeyInputProcessor;

namespace
{
static AFGPlayerController* ResolveLocalPlayerController()
{
	if (!GEngine || !GEngine->GameViewport)
	{
		return nullptr;
	}

	UWorld* World = GEngine->GameViewport->GetWorld();
	if (!IsValid(World) || !World->IsGameWorld())
	{
		return nullptr;
	}

	AFGPlayerController* PC = Cast<AFGPlayerController>(World->GetFirstPlayerController());
	if (!IsValid(PC) || !PC->IsLocalController())
	{
		return nullptr;
	}

	return PC;
}

class FStructuralPowerIdHotkeyProcessor : public IInputProcessor
{
public:
	virtual bool HandleKeyDownEvent(
		FSlateApplication& SlateApp,
		const FKeyEvent& InKeyEvent) override
	{
		if (InKeyEvent.GetKey() != EKeys::I)
		{
			return false;
		}

		if (UStructuralPowerIdConfigWidget* Active =
				UStructuralPowerIdConfigWidget::GetActiveWidget())
		{
			if (IsValid(Active) && Active->IsTextFieldFocused())
			{
				return false;
			}
		}

		AFGPlayerController* PC = ResolveLocalPlayerController();
		if (!IsValid(PC))
		{
			return false;
		}

		if (UFGGameUI* GameUI = PC->GetGameUI())
		{
			if (IsValid(GameUI) && GameUI->HasActiveInteractWidget())
			{
				return false;
			}
		}

		FStructuralPowerIdInput::OpenIdPanelForController(PC);
		return true;
	}

	virtual void Tick(
		const float DeltaTime,
		FSlateApplication& SlateApp,
		TSharedRef<ICursor> Cursor) override
	{
	}
};
}

void FStructuralPowerIdInput::EnsureHotkeyProcessorRegistered()
{
	if (HotkeyInputProcessor.IsValid())
	{
		return;
	}

	if (!FSlateApplication::IsInitialized())
	{
		return;
	}

	HotkeyInputProcessor = MakeShared<FStructuralPowerIdHotkeyProcessor>();
	FSlateApplication::Get().RegisterInputPreProcessor(HotkeyInputProcessor, 10);
	UE_LOG(LogStructuralPower, Log, TEXT("[PWR] Id panel hotkey processor registered (I)"));
}

void FStructuralPowerIdInput::UnregisterHotkeyProcessor()
{
	if (!HotkeyInputProcessor.IsValid())
	{
		return;
	}

	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(HotkeyInputProcessor);
	}

	HotkeyInputProcessor.Reset();
}

void FStructuralPowerIdInput::EnsureInputReady(AFGPlayerController* PlayerController)
{
	if (!IsValid(PlayerController) || !PlayerController->IsLocalController())
	{
		return;
	}

	UWorld* World = PlayerController->GetWorld();
	if (!IsValid(World) || World->bIsTearingDown)
	{
		return;
	}

	// Slate pre-processor only — FG/EnhancedInput mapping contexts crash during load travel.
	EnsureHotkeyProcessorRegistered();
}

void FStructuralPowerIdInput::Register()
{
	if (InputInitHandle.IsValid())
	{
		return;
	}

	InputInitHandle = AFGCharacterPlayer::OnPlayerInputInitialized.AddStatic(
		&FStructuralPowerIdInput::OnPlayerInputInitialized);
}

void FStructuralPowerIdInput::Unregister()
{
	UnregisterHotkeyProcessor();

	if (InputInitHandle.IsValid())
	{
		AFGCharacterPlayer::OnPlayerInputInitialized.Remove(InputInitHandle);
		InputInitHandle.Reset();
	}
}

void FStructuralPowerIdInput::OnPlayerInputInitialized(
	AFGCharacterPlayer* Player,
	UInputComponent* /*InputComponent*/)
{
	if (!IsValid(Player))
	{
		return;
	}

	AFGPlayerController* PlayerController = Cast<AFGPlayerController>(Player->GetController());
	if (!IsValid(PlayerController) || !PlayerController->IsLocalController())
	{
		return;
	}

	EnsureInputReady(PlayerController);
}

void FStructuralPowerIdInput::NotifyPanelOpened(AFGPlayerController* /*PlayerController*/)
{
}

void FStructuralPowerIdInput::NotifyPanelClosed(AFGPlayerController* PlayerController)
{
	EnsureGameInputUnblocked(PlayerController);
}

void FStructuralPowerIdInput::EnsureGameInputUnblocked(AFGPlayerController* PlayerController)
{
	UStructuralPowerIdConfigWidget::ForceReleaseAllModalState(PlayerController);
}

void FStructuralPowerIdInput::RecoverAfterVanillaUiClosed(AFGPlayerController* PlayerController)
{
	if (!IsValid(PlayerController) || !PlayerController->IsLocalController())
	{
		return;
	}

	if (UStructuralPowerIdConfigWidget* Window = UStructuralPowerIdConfigWidget::GetPooledWindow())
	{
		if (IsValid(Window) && Window->IsPanelVisible())
		{
			return;
		}
	}

	if (UFGGameUI* GameUI = PlayerController->GetGameUI())
	{
		if (IsValid(GameUI) && GameUI->HasActiveInteractWidget())
		{
			return;
		}
	}

	UStructuralPowerIdConfigWidget::ForceReleaseAllModalState(PlayerController);
	EnsureInputReady(PlayerController);

	UE_LOG(LogStructuralPower, Log, TEXT("[PWR] Id panel input recovered after vanilla UI"));
}

void FStructuralPowerIdInput::OpenIdPanelForController(AFGPlayerController* PlayerController)
{
	if (!IsValid(PlayerController) || !PlayerController->IsLocalController())
	{
		return;
	}

	UE_LOG(LogStructuralPower, Log, TEXT("[PWR] Id panel I key"));
	EnsureInputReady(PlayerController);

	AFGBuildable* Target = nullptr;
	if (!FStructuralIdConfigTarget::PickFromView(PlayerController, Target))
	{
		UE_LOG(LogStructuralPower, Log, TEXT("[PWR] Id panel — no eligible trace target"));
		return;
	}

	UStructuralPowerIdConfigWidget* Window =
		UStructuralPowerIdConfigWidget::GetOrCreateWindow(PlayerController);
	if (!IsValid(Window))
	{
		UE_LOG(LogStructuralPower, Warning, TEXT("[PWR] Id panel — failed to create window"));
		return;
	}

	Window->NormalizeModalState();

	if (Window->IsPanelVisible())
	{
		if (IsValid(Target) && Window->GetTargetBuildable() != Target)
		{
			Window->RetargetTo(Target);
			UE_LOG(LogStructuralPower, Log,
				TEXT("[PWR] Id panel retargeted via I key target=%s"),
				*Target->GetName());
		}
		else
		{
			Window->ClosePanel();
			UE_LOG(LogStructuralPower, Log, TEXT("[PWR] Id panel toggled closed"));
		}
		return;
	}

	Window->OpenForTarget(Target);
}
