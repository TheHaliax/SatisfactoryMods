// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Rules/FStructuralEligibilityRules.h"

#include "Buildables/FGBuildable.h"
#include "FGBuildableBeam.h"
#include "Buildables/FGBuildableCornerWall.h"
#include "Buildables/FGBuildableFactory.h"
#include "Buildables/FGBuildableFactoryBuilding.h"
#include "Buildables/FGBuildableFoundation.h"
#include "Buildables/FGBuildablePowerPole.h"
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

bool FStructuralEligibilityRules::IsWallOutlet(const AFGBuildable* Buildable)
{
	const AFGBuildablePowerPole* Pole = Cast<AFGBuildablePowerPole>(Buildable);
	if (!Pole)
	{
		return false;
	}

	const EPowerPoleType PoleType = Pole->GetPowerPoleType();
	return PoleType == EPowerPoleType::PPT_WALL || PoleType == EPowerPoleType::PPT_WALL_DOUBLE;
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

bool FStructuralEligibilityRules::IsValidOutletParent(const AFGBuildable* Parent)
{
	return IsBusMember(Parent);
}
