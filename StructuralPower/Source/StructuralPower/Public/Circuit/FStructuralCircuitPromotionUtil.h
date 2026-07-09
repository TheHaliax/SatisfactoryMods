// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class UFGPowerConnectionComponent;

class STRUCTURALPOWER_API FStructuralCircuitPromotionUtil
{
public:
	/** Connector belongs to a FG circuit group (membership, not necessarily powered). */
	static bool ComponentOnCircuit(const UFGPowerConnectionComponent* Component);

	static bool ComponentCarriesPower(const UFGPowerConnectionComponent* Component);

	/** Live feed — FG UFGPowerConnectionComponent::HasPower(). */
	static bool ConnectorSuppliesPower(const UFGPowerConnectionComponent* Component);

	/** Hidden edge exists but FG split circuits — repair via ConnectComponents only. */
	static bool HiddenLinkNeedsCircuitRepair(
		UFGPowerConnectionComponent* A,
		UFGPowerConnectionComponent* B);

	static void PromoteCircuitLink(
		UFGPowerConnectionComponent* A,
		UFGPowerConnectionComponent* B,
		ELogVerbosity::Type PromoteVerbosity = ELogVerbosity::Log);

	/** Split FG circuits created for a structural hidden edge, then remove the edge. */
	static void DemoteHiddenCircuitLink(
		UFGPowerConnectionComponent* A,
		UFGPowerConnectionComponent* B);
};
