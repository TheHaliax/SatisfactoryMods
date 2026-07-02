// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

namespace StructuralPowerConstants
{
	inline constexpr float OverlapPaddingCm = 10.0f;
	inline constexpr float DefaultFoundationExtentCm = 400.0f;
	inline constexpr float GridCellCm = 100.0f;
	inline constexpr float MaxOutletParentDistCm = 500.0f;
	inline constexpr float MaxOutletParentDistCmSq = MaxOutletParentDistCm * MaxOutletParentDistCm;
	inline constexpr float StructuralConnectivityGapCm = 8.0f;
	inline constexpr int32 MaxHiddenConnectionLinks = 255;
	inline constexpr int32 DeferredPlacementsPerTick = 15;
	inline constexpr int32 DiagnosticSampleCount = 10;

	inline const FName StructuralConnectorName = TEXT("StructuralPowerConnection");
	inline const FName OutletBusConnectorName = TEXT("StructuralPowerOutletBus");
}
