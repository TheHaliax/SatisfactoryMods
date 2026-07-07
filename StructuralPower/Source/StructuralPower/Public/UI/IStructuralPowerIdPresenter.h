// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Routing/EStructuralChannel.h"

class AFGBuildable;
class AFGPlayerController;

class STRUCTURALPOWER_API IStructuralPowerIdPresenter
{
public:
	virtual ~IStructuralPowerIdPresenter() = default;

	virtual void PrepareForController(AFGPlayerController* PlayerController) {}

	virtual void OpenForTarget(AFGBuildable* Target) = 0;
	virtual void RetargetTo(AFGBuildable* Target) = 0;
	virtual void Close() = 0;
	virtual void ResetForMapTravel() = 0;

	virtual bool IsOpen() const = 0;
	virtual bool IsTextFieldFocused() const = 0;

	virtual void ApplyComponentIdList(const FStructuralComponentIdList& List) = 0;

	virtual void RequestComponentIdList(AFGBuildable* Target) = 0;
	virtual void ApplyEndpointIds(
		AFGBuildable* Target,
		FName Source,
		FName Control,
		bool bClearSource,
		bool bClearControl) = 0;
};
