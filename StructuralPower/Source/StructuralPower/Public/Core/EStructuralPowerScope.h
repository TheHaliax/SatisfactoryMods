// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

enum class EStructuralPowerScope : uint8
{
	Site,
	CrossSite,
};

STRUCTURALPOWER_API const TCHAR* StructuralPowerScopeToString(EStructuralPowerScope Scope);
