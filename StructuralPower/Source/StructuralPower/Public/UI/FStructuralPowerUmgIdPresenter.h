// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "UI/IStructuralPowerIdPresenter.h"

class UStructuralPowerIdConfigWidget;

class STRUCTURALPOWER_API FStructuralPowerUmgIdPresenter final : public IStructuralPowerIdPresenter
{
public:
	void PrepareForController(AFGPlayerController* PlayerController) override;

	void OpenForTarget(AFGBuildable* Target) override;
	void RetargetTo(AFGBuildable* Target) override;
	void Close() override;
	void ResetForMapTravel() override;

	bool IsOpen() const override;
	bool IsTextFieldFocused() const override;

	void ApplyComponentIdList(const FStructuralComponentIdList& List) override;

	void RequestComponentIdList(AFGBuildable* Target) override;
	void ApplyEndpointIds(
		AFGBuildable* Target,
		FName Source,
		FName Control,
		bool bClearSource,
		bool bClearControl) override;

	void ForceReleaseModalState(
		AFGPlayerController* PlayerController,
		bool bRestoreGameInputMode);
	void ReleaseForVanillaInteract(AFGPlayerController* PlayerController);
	bool IsAnyPanelVisible() const;
	void NormalizeModalState();
	AFGBuildable* GetOpenTarget() const;

private:
	UStructuralPowerIdConfigWidget* ResolveWidgetForListUpdate() const;
	TWeakObjectPtr<AFGPlayerController> PreparedController;
};
