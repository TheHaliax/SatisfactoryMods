// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Equipment/FStructuralEquipmentBridgeRegistry.h"

#include "Core/FStructuralGraphSession.h"
#include "Equipment/FStructuralHoverpackEquipmentBridge.h"

FStructuralEquipmentBridgeRegistry& FStructuralEquipmentBridgeRegistry::Get()
{
	static FStructuralEquipmentBridgeRegistry Instance;
	return Instance;
}

void FStructuralEquipmentBridgeRegistry::Initialize()
{
	if (Bridges.Num() > 0)
	{
		return;
	}

	Register(MakeUnique<FStructuralHoverpackEquipmentBridge>());

	for (const TUniquePtr<IStructuralPowerEquipmentBridge>& Bridge : Bridges)
	{
		if (Bridge)
		{
			Bridge->RegisterHooks();
		}
	}
}

bool FStructuralEquipmentBridgeRegistry::QueryHoverpackStructuralAnchor(
	FStructuralGraphSession& Session,
	const FVector& QueryLoc,
	float MaxHorizontal,
	float MaxVertical,
	FStructuralHoverpackAnchorQuery& Out) const
{
	for (const TUniquePtr<IStructuralPowerEquipmentBridge>& Bridge : Bridges)
	{
		if (Bridge
			&& Bridge->QueryHoverpackStructuralAnchor(
				Session,
				QueryLoc,
				MaxHorizontal,
				MaxVertical,
				Out))
		{
			return true;
		}
	}

	return false;
}

void FStructuralEquipmentBridgeRegistry::Register(
	TUniquePtr<IStructuralPowerEquipmentBridge> Bridge)
{
	if (Bridge)
	{
		Bridges.Add(MoveTemp(Bridge));
	}
}
