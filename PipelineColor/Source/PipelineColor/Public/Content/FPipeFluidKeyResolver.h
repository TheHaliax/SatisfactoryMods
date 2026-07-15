// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Content/IContentKeyResolver.h"

class FPipeFluidKeyResolver final : public IContentKeyResolver
{
public:
	virtual bool Supports(AFGBuildable* Buildable) const override;
	virtual void Resolve(
		AFGBuildable* Buildable,
		TSubclassOf<UFGItemDescriptor>& OutFluid,
		bool& bOutEmpty) const override;

	static bool IsPipeColorTarget(AFGBuildable* Buildable);
};
