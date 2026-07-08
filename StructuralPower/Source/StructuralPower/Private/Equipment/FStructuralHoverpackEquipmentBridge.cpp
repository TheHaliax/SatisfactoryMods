// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Equipment/FStructuralHoverpackEquipmentBridge.h"

#include "Circuit/FStructuralCircuitPromotionUtil.h"
#include "Config/FStructuralPowerModConfig.h"
#include "Equipment/FStructuralHoverpackBridge.h"
#include "FGCircuitConnectionComponent.h"
#include "FGPowerConnectionComponent.h"
#include "Save/AStructuralPowerGraphSubsystem.h"
#include "Session/FStructuralPowerSessionSettings.h"

FName FStructuralHoverpackEquipmentBridge::GetKind() const
{
	return TEXT("Hoverpack");
}

void FStructuralHoverpackEquipmentBridge::RegisterHooks()
{
	FStructuralHoverpackBridge::RegisterHooks();
}

bool FStructuralHoverpackEquipmentBridge::QueryHoverpackStructuralAnchor(
	AStructuralPowerGraphSubsystem& Graph,
	const FVector& QueryLoc,
	float MaxHorizontal,
	float MaxVertical,
	FStructuralHoverpackAnchorQuery& Out) const
{
	Out = FStructuralHoverpackAnchorQuery{};

	if (MaxHorizontal <= 0.0f || MaxVertical <= 0.0f)
	{
		return false;
	}

	int32 ComponentRoot = INDEX_NONE;
	if (!Graph.FindNearestStructureAnchorForEquipment(
			QueryLoc,
			MaxHorizontal,
			MaxVertical,
			Out.Anchor,
			ComponentRoot))
	{
		return false;
	}

	Out.ComponentRoot = ComponentRoot;

	if (UFGCircuitConnectionComponent* Source = Graph.GetComponentSourceConnector(ComponentRoot, nullptr))
	{
		if (UFGPowerConnectionComponent* Feed = Cast<UFGPowerConnectionComponent>(Source))
		{
			Out.FeedConnector = Feed;
			Out.bPowered = FStructuralCircuitPromotionUtil::ConnectorSuppliesPower(Feed);
		}
	}

	Out.bFound = true;
	return true;
}
