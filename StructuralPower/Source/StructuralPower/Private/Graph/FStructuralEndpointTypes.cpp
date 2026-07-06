// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Graph/FStructuralEndpointTypes.h"

#include "Buildables/FGBuildableCircuitSwitch.h"

AFGBuildableCircuitSwitch* FTrackedEndpoint::GetSwitch() const
{
	return Cast<AFGBuildableCircuitSwitch>(Actor.Get());
}
