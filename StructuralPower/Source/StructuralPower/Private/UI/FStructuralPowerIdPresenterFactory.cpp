// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "UI/FStructuralPowerIdPresenterFactory.h"

#include "UI/FStructuralPowerUmgIdPresenter.h"

TUniquePtr<IStructuralPowerIdPresenter> FStructuralPowerIdPresenterFactory::ActivePresenter;

void FStructuralPowerIdPresenterFactory::EnsureDefaultImplementation()
{
	if (!ActivePresenter.IsValid())
	{
		ActivePresenter = MakeUnique<FStructuralPowerUmgIdPresenter>();
	}
}

IStructuralPowerIdPresenter& FStructuralPowerIdPresenterFactory::Get()
{
	EnsureDefaultImplementation();
	return *ActivePresenter;
}

void FStructuralPowerIdPresenterFactory::SetImplementation(
	TUniquePtr<IStructuralPowerIdPresenter> Impl)
{
	ActivePresenter = MoveTemp(Impl);
	EnsureDefaultImplementation();
}

void FStructuralPowerIdPresenterFactory::ResetForMapTravel()
{
	Get().ResetForMapTravel();
}

void FStructuralPowerIdPresenterFactory::CloseActive()
{
	Get().Close();
}

void FStructuralPowerIdPresenterFactory::ForceReleaseModalState(
	AFGPlayerController* PlayerController,
	const bool bRestoreGameInputMode)
{
	EnsureDefaultImplementation();
	if (FStructuralPowerUmgIdPresenter* Umg =
			static_cast<FStructuralPowerUmgIdPresenter*>(ActivePresenter.Get()))
	{
		Umg->ForceReleaseModalState(PlayerController, bRestoreGameInputMode);
	}
}

void FStructuralPowerIdPresenterFactory::ReleaseForVanillaInteract(
	AFGPlayerController* PlayerController)
{
	EnsureDefaultImplementation();
	if (FStructuralPowerUmgIdPresenter* Umg =
			static_cast<FStructuralPowerUmgIdPresenter*>(ActivePresenter.Get()))
	{
		Umg->ReleaseForVanillaInteract(PlayerController);
	}
}

void FStructuralPowerIdPresenterFactory::PrepareWindowForController(
	AFGPlayerController* PlayerController)
{
	Get().PrepareForController(PlayerController);
}

void FStructuralPowerIdPresenterFactory::NormalizeModalState()
{
	EnsureDefaultImplementation();
	if (FStructuralPowerUmgIdPresenter* Umg =
			static_cast<FStructuralPowerUmgIdPresenter*>(ActivePresenter.Get()))
	{
		Umg->NormalizeModalState();
	}
}

AFGBuildable* FStructuralPowerIdPresenterFactory::GetOpenTarget()
{
	EnsureDefaultImplementation();
	if (const FStructuralPowerUmgIdPresenter* Umg =
			static_cast<const FStructuralPowerUmgIdPresenter*>(ActivePresenter.Get()))
	{
		return Umg->GetOpenTarget();
	}

	return nullptr;
}

bool FStructuralPowerIdPresenterFactory::IsAnyPanelVisible()
{
	EnsureDefaultImplementation();
	if (const FStructuralPowerUmgIdPresenter* Umg =
			static_cast<const FStructuralPowerUmgIdPresenter*>(ActivePresenter.Get()))
	{
		return Umg->IsAnyPanelVisible();
	}

	return Get().IsOpen();
}

bool FStructuralPowerIdPresenterFactory::IsTextFieldFocused()
{
	return Get().IsTextFieldFocused();
}
