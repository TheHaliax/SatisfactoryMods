// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Equipment/IStructuralPowerEquipmentBridge.h"

class STRUCTURALPOWER_API FStructuralHoverpackEquipmentBridge : public IStructuralPowerEquipmentBridge
{
public:
	virtual FName GetKind() const override;

	virtual void RegisterHooks() override;

	virtual bool QueryHoverpackStructuralAnchor(
		AStructuralPowerGraphSubsystem& Graph,
		const FVector& QueryLoc,
		float MaxHorizontal,
		float MaxVertical,
		FStructuralHoverpackAnchorQuery& Out) const override;
};
