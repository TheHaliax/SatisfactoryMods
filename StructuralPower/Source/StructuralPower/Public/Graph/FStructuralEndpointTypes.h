// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Core/FStructuralNodeId.h"

class AFGBuildable;
class AFGBuildablePowerPole;

enum class EStructuralEndpointKind : uint8
{
	Pole,
	Switch
};

struct FTrackedEndpoint
{
	TWeakObjectPtr<AFGBuildable> Actor;
	FStructuralNodeId ParentId;
	EStructuralEndpointKind Kind = EStructuralEndpointKind::Pole;

	AFGBuildablePowerPole* GetPole() const;
	class AFGBuildableCircuitSwitch* GetSwitch() const;
};
