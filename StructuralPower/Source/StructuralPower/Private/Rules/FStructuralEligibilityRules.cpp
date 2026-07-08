// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Rules/FStructuralEligibilityRules.h"

#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Buildables/FGBuildableGenerator.h"
#include "Buildables/FGBuildableLightSource.h"
#include "Buildables/FGBuildableLightsControlPanel.h"
#include "Buildables/FGBuildableManufacturer.h"
#include "Buildables/FGBuildableResourceExtractorBase.h"
#include "FGBuildableBeam.h"
#include "Routing/EStructuralChannel.h"
#include "Buildables/FGBuildableCornerWall.h"
#include "Buildables/FGBuildableFactory.h"
#include "Buildables/FGBuildableFactoryBuilding.h"
#include "Buildables/FGBuildableFoundation.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "Buildables/FGBuildablePowerStorage.h"
#include "Buildables/FGBuildableRamp.h"
#include "Buildables/FGBuildableStair.h"
#include "Buildables/FGBuildableWalkway.h"
#include "Buildables/FGBuildableWall.h"
#include "FGBuildablePillar.h"
#include "FGPowerInfoComponent.h"

bool FStructuralEligibilityRules::IsBusMember(const AFGBuildable* Buildable)
{
	if (!IsValid(Buildable) || Buildable->IsA<AFGBuildableFactory>())
	{
		return false;
	}

	if (Buildable->IsA<AFGBuildablePowerPole>())
	{
		return false;
	}

	TInlineComponentArray<UFGPowerInfoComponent*> PowerInfos;
	const_cast<AFGBuildable*>(Buildable)->GetComponents(PowerInfos);
	if (PowerInfos.Num() > 0)
	{
		return false;
	}

	// Mod structural packs often inherit factory building without UFGPowerInfoComponent.
	return Buildable->IsA<AFGBuildableFactoryBuilding>()
		|| Buildable->IsA<AFGBuildableFoundation>()
		|| Buildable->IsA<AFGBuildableRamp>()
		|| Buildable->IsA<AFGBuildableStair>()
		|| Buildable->IsA<AFGBuildableWalkway>()
		|| Buildable->IsA<AFGBuildableWall>()
		|| Buildable->IsA<AFGBuildableCornerWall>()
		|| Buildable->IsA<AFGBuildableBeam>()
		|| Buildable->IsA<AFGBuildablePillar>();
}

bool FStructuralEligibilityRules::IsPowerBridgeSwitch(const AFGBuildable* Buildable)
{
	return IsValid(Buildable) && Buildable->IsA<AFGBuildableCircuitSwitch>();
}

bool FStructuralEligibilityRules::IsStructuralLightConsumer(const AFGBuildable* Buildable)
{
	return IsValid(Buildable)
		&& Buildable->IsA<AFGBuildableLightSource>()
		&& !Buildable->IsA<AFGBuildableLightsControlPanel>();
}

bool FStructuralEligibilityRules::IsIdConfigTarget(const AFGBuildable* Buildable)
{
	if (!IsValid(Buildable))
	{
		return false;
	}

	if (IsStructuralLightConsumer(Buildable))
	{
		return true;
	}

	if (IsPowerBridgeSwitch(Buildable))
	{
		return true;
	}

	return Buildable->IsA<AFGBuildableLightsControlPanel>();
}

bool FStructuralEligibilityRules::IsPowerBridgePole(const AFGBuildable* Buildable)
{
	const AFGBuildablePowerPole* Pole = Cast<AFGBuildablePowerPole>(Buildable);
	if (!Pole)
	{
		return false;
	}

	switch (Pole->GetPowerPoleType())
	{
	case EPowerPoleType::PPT_WALL:
	case EPowerPoleType::PPT_WALL_DOUBLE:
	case EPowerPoleType::PPT_POLE:
	case EPowerPoleType::PPT_TOWER:
		return true;
	default:
		return false;
	}
}

bool FStructuralEligibilityRules::IsPowerStorage(const AFGBuildable* Buildable)
{
	return IsValid(Buildable) && Buildable->IsA<AFGBuildablePowerStorage>();
}

bool FStructuralEligibilityRules::IsValidOutletParent(const AFGBuildable* Parent)
{
	return IsBusMember(Parent);
}

EStructuralChannel FStructuralEligibilityRules::ClassifyBuildable(const AFGBuildable* Buildable)
{
	if (!IsValid(Buildable))
	{
		return EStructuralChannel::Structure;
	}

	if (Buildable->IsA<AFGBuildableCircuitSwitch>())
	{
		return EStructuralChannel::Switch;
	}

	if (Buildable->IsA<AFGBuildableLightsControlPanel>()
		|| Buildable->IsA<AFGBuildableLightSource>())
	{
		return EStructuralChannel::Light;
	}

	if (Buildable->IsA<AFGBuildableGenerator>())
	{
		return EStructuralChannel::Generator;
	}

	if (Buildable->IsA<AFGBuildableResourceExtractorBase>())
	{
		return EStructuralChannel::Extractor;
	}

	if (Buildable->IsA<AFGBuildableManufacturer>())
	{
		return EStructuralChannel::Manufacturer;
	}

	return EStructuralChannel::Structure;
}
