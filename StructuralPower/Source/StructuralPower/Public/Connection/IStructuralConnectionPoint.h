// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Lightweight/FStructuralLightweightTypes.h"

class UFGPowerConnectionComponent;

struct STRUCTURALPOWER_API IStructuralConnectionPoint
{
	virtual ~IStructuralConnectionPoint() = default;

	virtual UFGPowerConnectionComponent* GetStructuralConnector() = 0;
	virtual FStructuralWallAnchor GetStructureAnchor() const = 0;

	/** Wire or gate delta — local bridge attach only; no site remesh. */
	virtual void OnWireOrGateChanged() = 0;
};
