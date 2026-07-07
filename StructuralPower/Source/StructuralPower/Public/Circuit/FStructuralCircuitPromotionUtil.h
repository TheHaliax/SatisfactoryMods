// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class UFGPowerConnectionComponent;

class STRUCTURALPOWER_API FStructuralCircuitPromotionUtil
{
public:
	static bool ComponentCarriesPower(const UFGPowerConnectionComponent* Component);

	static bool ConnectorSuppliesPower(const UFGPowerConnectionComponent* Component);

	static void PromoteCircuitLink(
		UFGPowerConnectionComponent* A,
		UFGPowerConnectionComponent* B,
		ELogVerbosity::Type PromoteVerbosity = ELogVerbosity::Log);
};
