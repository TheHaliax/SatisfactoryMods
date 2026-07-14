// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Equipment/FStructuralHoverpackEquipmentBridge.h"

#include "Circuit/FStructuralCircuitPromotionUtil.h"
#include "Core/FStructuralGraphSession.h"
#include "Circuit/FStructuralGraphCircuitOps.h"
#include "Equipment/FStructuralHoverpackBridge.h"
#include "FGCircuitConnectionComponent.h"
#include "FGPowerConnectionComponent.h"

FName FStructuralHoverpackEquipmentBridge::GetKind() const
{
	return TEXT("Hoverpack");
}

void FStructuralHoverpackEquipmentBridge::RegisterHooks()
{
	FStructuralHoverpackBridge::RegisterHooks();
}

bool FStructuralHoverpackEquipmentBridge::QueryHoverpackStructuralAnchor(
	FStructuralGraphSession& Session,
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
	if (!Session.FindNearestStructureAnchorForEquipment(
			QueryLoc,
			MaxHorizontal,
			MaxVertical,
			Out.Anchor,
			ComponentRoot))
	{
		return false;
	}

	Out.ComponentRoot = ComponentRoot;

	if (UFGCircuitConnectionComponent* Source = Session.Circuit().GetComponentSourceConnector(ComponentRoot, nullptr))
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
