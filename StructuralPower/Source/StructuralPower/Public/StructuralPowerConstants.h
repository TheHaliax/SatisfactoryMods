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
	// Load-time link revalidation: only cut saved structure links whose bounds are this
	// far apart (clearly non-contiguous "wireless" bridges). Valid adjacency incl. thin/
	// beam/edge pieces sits well under this, so contiguous meshes are never fragmented.
	inline constexpr float MaxStructuralLinkValidateGapCm = 1000.0f;
	inline constexpr float GroundPoleFoundationVerticalReachCm = 800.0f;
	inline constexpr float GroundPoleFoundationVerticalBandBelowCm = 200.0f;
	inline constexpr int32 PassiveEnergizeMaxHiddenHops = 256;
	inline constexpr float StructuralConnectivityGapCm = 8.0f;
	inline constexpr int32 MaxHiddenConnectionLinks = 255;
	inline constexpr int32 DeferredPlacementsPerTick = 15;
	inline constexpr int32 DiagnosticSampleCount = 10;

	inline const FName StructuralConnectorName = TEXT("StructuralPowerConnection");
	inline const FName OutletBusConnectorName = TEXT("StructuralPowerOutletBus");
}
