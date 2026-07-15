// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Resources/FGItemDescriptor.h"
#include "Templates/SubclassOf.h"

class AFGBuildable;

class IContentKeyResolver
{
public:
	virtual ~IContentKeyResolver() = default;

	virtual bool Supports(AFGBuildable* Buildable) const = 0;
	virtual void Resolve(
		AFGBuildable* Buildable,
		TSubclassOf<UFGItemDescriptor>& OutFluid,
		bool& bOutEmpty) const = 0;
};
