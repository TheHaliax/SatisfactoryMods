// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Graph/FStructuralEndpointTypes.h"

#include "Graph/FStructuralPowerBuildableCasts.h"

AFGBuildableCircuitSwitch* FTrackedEndpoint::GetSwitch() const
{
	return FStructuralPowerBuildableCasts::AsSwitch(Actor.Get());
}

AFGBuildablePowerPole* FTrackedEndpoint::GetPole() const
{
	return FStructuralPowerBuildableCasts::AsPole(Actor.Get());
}

AFGBuildableLightSource* FTrackedEndpoint::GetLight() const
{
	return FStructuralPowerBuildableCasts::AsLight(Actor.Get());
}

AFGBuildableLightsControlPanel* FTrackedEndpoint::GetPanel() const
{
	return FStructuralPowerBuildableCasts::AsPanel(Actor.Get());
}

AFGBuildablePowerStorage* FTrackedEndpoint::GetStorage() const
{
	return FStructuralPowerBuildableCasts::AsStorage(Actor.Get());
}
