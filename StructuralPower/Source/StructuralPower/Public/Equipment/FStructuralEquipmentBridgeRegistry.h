// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Equipment/IStructuralPowerEquipmentBridge.h"

class FStructuralGraphSession;
struct FStructuralHoverpackAnchorQuery;

class STRUCTURALPOWER_API FStructuralEquipmentBridgeRegistry
{
public:
	static FStructuralEquipmentBridgeRegistry& Get();

	FStructuralEquipmentBridgeRegistry() = default;
	FStructuralEquipmentBridgeRegistry(const FStructuralEquipmentBridgeRegistry&) = delete;
	FStructuralEquipmentBridgeRegistry& operator=(const FStructuralEquipmentBridgeRegistry&) = delete;

	void Initialize();

	bool QueryHoverpackStructuralAnchor(
		FStructuralGraphSession& Session,
		const FVector& QueryLoc,
		float MaxHorizontal,
		float MaxVertical,
		FStructuralHoverpackAnchorQuery& Out) const;

private:
	void Register(TUniquePtr<IStructuralPowerEquipmentBridge> Bridge);

	TArray<TUniquePtr<IStructuralPowerEquipmentBridge>> Bridges;
};
