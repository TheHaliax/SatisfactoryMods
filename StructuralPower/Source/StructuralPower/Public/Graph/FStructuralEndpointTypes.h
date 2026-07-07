// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Core/FStructuralNodeId.h"
#include "Routing/EStructuralChannel.h"

class AFGBuildable;

enum class EStructuralEndpointKind : uint8
{
	Pole,
	Switch,
	Light,
	Panel
};

struct FTrackedEndpoint
{
	TWeakObjectPtr<AFGBuildable> Actor;
	FStructuralNodeId ParentId;
	EStructuralEndpointKind Kind = EStructuralEndpointKind::Pole;

	/** Panel restitch idempotency — skip tear-down/relink when routing state unchanged. */
	FStructuralChannelKey CachedPanelKey;
	int32 CachedPanelRoot = INDEX_NONE;
	bool bPanelLinksReady = false;

	/** Downstream keyed subnet — skip relink when control id unchanged. */
	FName CachedDownstreamControl = NAME_None;
	bool bDownstreamLinksReady = false;

	class AFGBuildableCircuitSwitch* GetSwitch() const;
	class AFGBuildablePowerPole* GetPole() const;
	class AFGBuildableLightSource* GetLight() const;
	class AFGBuildableLightsControlPanel* GetPanel() const;
};
