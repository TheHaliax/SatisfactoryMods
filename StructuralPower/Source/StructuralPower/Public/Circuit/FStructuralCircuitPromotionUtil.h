// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class UFGPowerConnectionComponent;

class STRUCTURALPOWER_API FStructuralCircuitPromotionUtil
{
public:
	static bool ComponentOnCircuit(const UFGPowerConnectionComponent* Component);

	static bool ComponentCarriesPower(const UFGPowerConnectionComponent* Component);

	static bool ConnectorSuppliesPower(const UFGPowerConnectionComponent* Component);

	static bool HiddenLinkNeedsCircuitRepair(
		UFGPowerConnectionComponent* A,
		UFGPowerConnectionComponent* B);

	static void PromoteCircuitLink(
		UFGPowerConnectionComponent* A,
		UFGPowerConnectionComponent* B,
		ELogVerbosity::Type PromoteVerbosity = ELogVerbosity::Log);

	static void DemoteHiddenCircuitLink(
		UFGPowerConnectionComponent* A,
		UFGPowerConnectionComponent* B);
};
