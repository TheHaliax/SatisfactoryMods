// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Graph/FStructuralSiteContext.h"

class AFGBuildable;
class AStructuralPowerGraphSubsystem;
class UFGStructuralPowerConnectionComponent;
struct FStructuralPowerContext;

struct FStructuralBridgeAttachRequest
{
	AFGBuildable* Host = nullptr;
	EStructuralEndpointKind Kind = EStructuralEndpointKind::Pole;
	bool bUsePoleRootResolver = false;
};

struct FStructuralBridgeAttachOutcome
{
	bool bAttached = false;
	bool bSiteNotReady = false;
	FStructuralSiteContext Site;
	UFGStructuralPowerConnectionComponent* OutletBus = nullptr;
};

/** Shared place/load attach for non-toggle bridge endpoints (pole, storage). */
class STRUCTURALPOWER_API FStructuralBridgeAttach
{
public:
	static bool HasPlacementMembership(
		AStructuralPowerGraphSubsystem& Graph,
		AFGBuildable* Host,
		EStructuralEndpointKind ExpectedKind);

	static FStructuralBridgeAttachOutcome AttachOnPlace(
		FStructuralPowerContext& Ctx,
		const FStructuralBridgeAttachRequest& Request);
};
