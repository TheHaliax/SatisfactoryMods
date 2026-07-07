// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Processors/FStructuralPowerProcessorRegistry.h"

#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableLightsControlPanel.h"
#include "Config/FStructuralPowerModConfig.h"
#include "Processors/IStructuralPowerProcessor.h"
#include "Rules/FStructuralEligibilityRules.h"

FStructuralPowerProcessorRegistry& FStructuralPowerProcessorRegistry::Get()
{
	static FStructuralPowerProcessorRegistry Instance;
	return Instance;
}

const IStructuralPowerProcessor* FStructuralPowerProcessorRegistry::Find(
	const EStructuralEndpointKind Kind) const
{
	const TUniquePtr<IStructuralPowerProcessor>* Found = ByKind.Find(Kind);
	return Found ? Found->Get() : nullptr;
}

IStructuralPowerProcessor* FStructuralPowerProcessorRegistry::FindMutable(
	const EStructuralEndpointKind Kind)
{
	TUniquePtr<IStructuralPowerProcessor>* Found = ByKind.Find(Kind);
	return Found ? Found->Get() : nullptr;
}

const IStructuralPowerProcessor* FStructuralPowerProcessorRegistry::FindForBuildable(
	const AFGBuildable* Buildable) const
{
	if (!IsValid(Buildable))
	{
		return nullptr;
	}

	if (FStructuralEligibilityRules::IsStructuralLightConsumer(Buildable))
	{
		return Find(EStructuralEndpointKind::Light);
	}

	if (Buildable->IsA<AFGBuildableLightsControlPanel>())
	{
		return Find(EStructuralEndpointKind::Panel);
	}

	if (FStructuralPowerModConfig::IsGatePowerSwitchesEnabled()
		&& FStructuralEligibilityRules::IsPowerBridgeSwitch(Buildable))
	{
		return Find(EStructuralEndpointKind::Switch);
	}

	if (FStructuralEligibilityRules::IsPowerBridgePole(Buildable))
	{
		return Find(EStructuralEndpointKind::Pole);
	}

	return nullptr;
}
