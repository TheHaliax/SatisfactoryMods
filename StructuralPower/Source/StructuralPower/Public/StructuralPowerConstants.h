// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

namespace StructuralPowerConstants
{
	inline constexpr float OverlapPaddingCm = 10.0f;
	inline constexpr float BeamOverlapPaddingCm = 50.0f;
	inline constexpr float BeamStructuralGapCm = 32.0f;
	inline constexpr float DefaultFoundationExtentCm = 400.0f;
	inline constexpr float GridCellCm = 100.0f;
	inline constexpr float MaxOutletParentDistCm = 1500.0f;
	inline constexpr float MaxOutletParentDistCmSq = MaxOutletParentDistCm * MaxOutletParentDistCm;
	inline constexpr float GroundPoleFoundationVerticalReachCm = 800.0f;
	inline constexpr float GroundPoleFoundationVerticalBandBelowCm = 200.0f;
	inline constexpr float StructuralConnectivityGapCm = 8.0f;
	inline constexpr int32 MaxHiddenConnectionLinks = 255;
	inline constexpr int32 DeferredPlacementsPerTick = 15;

	inline const FName OutletBusConnectorName = TEXT("StructuralPowerOutletBus");
	/** Per-panel keyed downstream bridge — never meshed with outlet bus (DR-006). */
	inline const FName PanelControlBusConnectorName = TEXT("StructuralPowerPanelControlBus");

	/** DR-011/012 fixed sentinels — not valid player-chosen custom names (DR-014). */
	inline const FName ControlBypass = TEXT("BYPASS");
	inline const FName ControlUnconfigured = TEXT("UNCONFIGURED");
}
