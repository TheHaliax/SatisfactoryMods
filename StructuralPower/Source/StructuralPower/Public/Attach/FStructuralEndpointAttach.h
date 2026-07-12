// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Core/EAttachContext.h"
#include "Graph/FStructuralEndpointDescriptor.h"

class AFGBuildable;
struct FStructuralBridgeAttachOutcome;
struct FStructuralBridgeAttachRequest;
struct FStructuralPowerContext;

class STRUCTURALPOWER_API FStructuralEndpointAttach
{
public:
	static FStructuralBridgeAttachOutcome AttachOnPlace(
		FStructuralPowerContext& Ctx,
		const FStructuralBridgeAttachRequest& Request);

	static bool AttachConsumer(FStructuralPowerContext& Ctx, AFGBuildable* Host, bool bLocalPromoteOnly);
	static bool AttachRouter(FStructuralPowerContext& Ctx, AFGBuildable* Host, bool bLocalPromoteOnly);
	static bool AttachToggleBridge(FStructuralPowerContext& Ctx, AFGBuildable* Host);

	static bool RunStrategy(
		FStructuralPowerContext& Ctx,
		AFGBuildable* Host,
		EStructuralAttachStrategy Strategy,
		bool bLocalPromoteOnly = false);
};
