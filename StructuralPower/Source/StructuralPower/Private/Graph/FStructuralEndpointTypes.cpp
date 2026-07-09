// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Graph/FStructuralEndpointTypes.h"

#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Buildables/FGBuildableLightSource.h"
#include "Buildables/FGBuildableLightsControlPanel.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "Buildables/FGBuildablePowerStorage.h"
#include "Graph/FStructuralPowerBuildableCasts.h"

const TCHAR* StructuralEndpointKindToString(const EStructuralEndpointKind Kind)
{
	switch (Kind)
	{
	case EStructuralEndpointKind::Pole: return TEXT("pole");
	case EStructuralEndpointKind::Switch: return TEXT("switch");
	case EStructuralEndpointKind::Light: return TEXT("light");
	case EStructuralEndpointKind::Panel: return TEXT("panel");
	case EStructuralEndpointKind::Storage: return TEXT("storage");
	default: return TEXT("?");
	}
}

AFGBuildableCircuitSwitch* FTrackedEndpoint::GetSwitch() const
{
	return Kind == EStructuralEndpointKind::Switch
		? FStructuralPowerBuildableCasts::AsSwitch(Actor.Get())
		: nullptr;
}

AFGBuildablePowerPole* FTrackedEndpoint::GetPole() const
{
	return Kind == EStructuralEndpointKind::Pole
		? FStructuralPowerBuildableCasts::AsPole(Actor.Get())
		: nullptr;
}

AFGBuildableLightSource* FTrackedEndpoint::GetLight() const
{
	return Kind == EStructuralEndpointKind::Light
		? FStructuralPowerBuildableCasts::AsLight(Actor.Get())
		: nullptr;
}

AFGBuildableLightsControlPanel* FTrackedEndpoint::GetPanel() const
{
	return Kind == EStructuralEndpointKind::Panel
		? FStructuralPowerBuildableCasts::AsPanel(Actor.Get())
		: nullptr;
}

AFGBuildablePowerStorage* FTrackedEndpoint::GetStorage() const
{
	return Kind == EStructuralEndpointKind::Storage
		? FStructuralPowerBuildableCasts::AsStorage(Actor.Get())
		: nullptr;
}
