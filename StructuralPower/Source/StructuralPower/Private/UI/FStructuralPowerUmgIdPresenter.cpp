// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "UI/FStructuralPowerUmgIdPresenter.h"

#include "Buildables/FGBuildable.h"
#include "FGPlayerController.h"
#include "UI/FStructuralPowerIdApplyBridge.h"
#include "UI/UStructuralPowerIdConfigWidget.h"

void FStructuralPowerUmgIdPresenter::PrepareForController(AFGPlayerController* PlayerController)
{
	if (!IsValid(PlayerController))
	{
		return;
	}

	PreparedController = PlayerController;
	UStructuralPowerIdConfigWidget::GetOrCreateWindow(PlayerController);
}

void FStructuralPowerUmgIdPresenter::OpenForTarget(AFGBuildable* Target)
{
	if (AFGPlayerController* PC = PreparedController.Get())
	{
		UStructuralPowerIdConfigWidget::GetOrCreateWindow(PC);
	}

	if (UStructuralPowerIdConfigWidget* Window = UStructuralPowerIdConfigWidget::GetPooledWindow())
	{
		Window->NormalizeModalState();
		Window->OpenForTarget(Target);
	}
}

void FStructuralPowerUmgIdPresenter::RetargetTo(AFGBuildable* Target)
{
	if (UStructuralPowerIdConfigWidget* Window = UStructuralPowerIdConfigWidget::GetActiveWidget())
	{
		Window->RetargetTo(Target);
	}
}

void FStructuralPowerUmgIdPresenter::Close()
{
	UStructuralPowerIdConfigWidget::CloseActivePanel();
}

void FStructuralPowerUmgIdPresenter::ResetForMapTravel()
{
	UStructuralPowerIdConfigWidget::ResetStaticsForMapLoad();
	PreparedController.Reset();
}

bool FStructuralPowerUmgIdPresenter::IsOpen() const
{
	if (UStructuralPowerIdConfigWidget* Active = UStructuralPowerIdConfigWidget::GetActiveWidget())
	{
		return IsValid(Active) && Active->IsPanelVisible();
	}

	return false;
}

bool FStructuralPowerUmgIdPresenter::IsTextFieldFocused() const
{
	if (UStructuralPowerIdConfigWidget* Active = UStructuralPowerIdConfigWidget::GetActiveWidget())
	{
		return IsValid(Active) && Active->IsTextFieldFocused();
	}

	return false;
}

UStructuralPowerIdConfigWidget* FStructuralPowerUmgIdPresenter::ResolveWidgetForListUpdate() const
{
	if (UStructuralPowerIdConfigWidget* Active = UStructuralPowerIdConfigWidget::GetActiveWidget())
	{
		return Active;
	}

	return UStructuralPowerIdConfigWidget::GetPooledWindow();
}

void FStructuralPowerUmgIdPresenter::ApplyComponentIdList(const FStructuralComponentIdList& List)
{
	if (UStructuralPowerIdConfigWidget* Widget = ResolveWidgetForListUpdate())
	{
		Widget->ApplyComponentIdList(List);
	}
}

void FStructuralPowerUmgIdPresenter::RequestComponentIdList(AFGBuildable* Target)
{
	if (AFGPlayerController* PC = PreparedController.Get())
	{
		FStructuralPowerIdApplyBridge::RequestComponentIdList(PC, Target);
		return;
	}

	if (UStructuralPowerIdConfigWidget* Widget = ResolveWidgetForListUpdate())
	{
		if (AFGPlayerController* PC = Widget->GetOwningPlayer<AFGPlayerController>())
		{
			FStructuralPowerIdApplyBridge::RequestComponentIdList(PC, Target);
		}
	}
}

void FStructuralPowerUmgIdPresenter::ApplyEndpointIds(
	AFGBuildable* Target,
	const FName Source,
	const FName Control,
	const bool bClearSource,
	const bool bClearControl)
{
	if (AFGPlayerController* PC = PreparedController.Get())
	{
		FStructuralPowerIdApplyBridge::ApplyEndpointIds(
			PC, Target, Source, Control, bClearSource, bClearControl);
		return;
	}

	if (UStructuralPowerIdConfigWidget* Widget = ResolveWidgetForListUpdate())
	{
		if (AFGPlayerController* PC = Widget->GetOwningPlayer<AFGPlayerController>())
		{
			FStructuralPowerIdApplyBridge::ApplyEndpointIds(
				PC, Target, Source, Control, bClearSource, bClearControl);
		}
	}
}

void FStructuralPowerUmgIdPresenter::ForceReleaseModalState(
	AFGPlayerController* PlayerController,
	const bool bRestoreGameInputMode)
{
	UStructuralPowerIdConfigWidget::ForceReleaseAllModalState(
		PlayerController, bRestoreGameInputMode);
}

void FStructuralPowerUmgIdPresenter::ReleaseForVanillaInteract(AFGPlayerController* PlayerController)
{
	UStructuralPowerIdConfigWidget::ReleaseForVanillaInteract(PlayerController);
}

bool FStructuralPowerUmgIdPresenter::IsAnyPanelVisible() const
{
	if (UStructuralPowerIdConfigWidget* Window = UStructuralPowerIdConfigWidget::GetPooledWindow())
	{
		return IsValid(Window) && Window->IsPanelVisible();
	}

	return false;
}

void FStructuralPowerUmgIdPresenter::NormalizeModalState()
{
	if (UStructuralPowerIdConfigWidget* Window = UStructuralPowerIdConfigWidget::GetPooledWindow())
	{
		Window->NormalizeModalState();
	}
}

AFGBuildable* FStructuralPowerUmgIdPresenter::GetOpenTarget() const
{
	if (UStructuralPowerIdConfigWidget* Window = UStructuralPowerIdConfigWidget::GetPooledWindow())
	{
		if (IsValid(Window) && Window->IsPanelVisible())
		{
			return Window->GetTargetBuildable();
		}
	}

	return nullptr;
}
