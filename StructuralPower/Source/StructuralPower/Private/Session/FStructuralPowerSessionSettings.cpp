// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Session/FStructuralPowerSessionSettings.h"

#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarStructuralPowerEnablePropagation(
	TEXT("StructuralPower.EnablePropagation"),
	1,
	TEXT("1 = enable structural power propagation; 0 = disable"),
	ECVF_Default);

bool FStructuralPowerSessionSettings::IsPropagationEnabled()
{
	return CVarStructuralPowerEnablePropagation.GetValueOnGameThread() != 0;
}
