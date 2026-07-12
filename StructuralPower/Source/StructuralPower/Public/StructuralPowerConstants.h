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
	inline constexpr float MaxStructuralAttachDistCm = 400.0f;
	inline constexpr float MaxStructuralAttachDistCmSq =
		MaxStructuralAttachDistCm * MaxStructuralAttachDistCm;
	inline constexpr float GroundPoleFoundationVerticalReachCm = 800.0f;
	inline constexpr float GroundPoleFoundationVerticalBandBelowCm = 200.0f;
	inline constexpr float StructuralConnectivityGapCm = 8.0f;
	inline constexpr int32 MaxHiddenConnectionLinks = 255;
	inline constexpr int32 DeferredPlacementsPerTick = 15;
	/** Bulk load: time budget is the limiter, not a low job cap. */
	inline constexpr int32 BulkDeferredPlacementsMaxJobs = 65536;
	inline constexpr double BulkLoadDrainTickBudgetSec = 0.050;

	inline const FName OutletBusConnectorName = TEXT("StructuralPowerOutletBus");
	// Panel control bus must never mesh the outlet bus — vanilla E on panel bleeds power both ways.
	inline const FName PanelControlBusConnectorName = TEXT("StructuralPowerPanelControlBus");

	inline const FName SwitchControlBusConnectorName = TEXT("StructuralPowerSwitchControlBus");

	inline const FName ControlUnconfigured = TEXT("UNCONFIGURED");
}
