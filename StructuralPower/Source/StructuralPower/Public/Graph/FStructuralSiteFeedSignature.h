// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

/** Gateway-only feed epoch for one structure site (component root). */
struct FStructuralSiteFeedSignature
{
	bool bPowered = false;
	int32 CircuitId = INDEX_NONE;

	bool operator==(const FStructuralSiteFeedSignature& Other) const
	{
		return bPowered == Other.bPowered && CircuitId == Other.CircuitId;
	}

	bool operator!=(const FStructuralSiteFeedSignature& Other) const
	{
		return !(*this == Other);
	}

	friend uint32 GetTypeHash(const FStructuralSiteFeedSignature& Signature)
	{
		return HashCombine(
			GetTypeHash(Signature.bPowered),
			GetTypeHash(Signature.CircuitId));
	}
};
