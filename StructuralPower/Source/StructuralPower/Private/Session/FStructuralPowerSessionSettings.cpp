// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Session/FStructuralPowerSessionSettings.h"

#include "Config/FStructuralPowerModConfig.h"

bool FStructuralPowerSessionSettings::IsPropagationEnabled()
{
	return FStructuralPowerModConfig::IsPropagationEnabled();
}
