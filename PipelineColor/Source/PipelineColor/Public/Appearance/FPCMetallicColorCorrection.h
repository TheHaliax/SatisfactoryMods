// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "Appearance/FPCAppearanceSpec.h"
#include "Math/Color.h"
#include "Math/UnrealMathUtility.h"

namespace FPCMetallicColorCorrection
{
constexpr float AlphaMin = 0.15f;
constexpr float AlphaMax = 0.45f;
constexpr float NeutralBoost = 1.35f;
constexpr float SilverFloor = 0.75f;
constexpr float SilverCeil = 1.0f;
constexpr float DarkFloor = 0.30f;
constexpr float SatSoftLo = 0.05f;
constexpr float SatSoftHi = 0.22f;

inline float LuminanceRec709(const FLinearColor& C)
{
	return 0.2126f * C.R + 0.7152f * C.G + 0.0722f * C.B;
}

inline float RoughnessFromLuminance(float Y)
{
	const float YClamped = FMath::Max(Y, 0.f);
	return AlphaMin + (AlphaMax - AlphaMin) * (YClamped * YClamped);
}

inline float Saturation(const FLinearColor& C)
{
	const float CMax = FMath::Max3(C.R, C.G, C.B);
	if (CMax <= KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}
	const float CMin = FMath::Min3(C.R, C.G, C.B);
	return (CMax - CMin) / CMax;
}

inline FLinearColor LiftDarkPreserveHue(const FLinearColor& C)
{
	const float CMax = FMath::Max3(C.R, C.G, C.B);
	if (CMax >= DarkFloor || CMax <= KINDA_SMALL_NUMBER)
	{
		return C;
	}
	const float Scale = DarkFloor / CMax;
	return FLinearColor(C.R * Scale, C.G * Scale, C.B * Scale, C.A);
}

inline float NeutralWeight(float S)
{
	return 1.f - FMath::SmoothStep(SatSoftLo, SatSoftHi, S);
}

inline FLinearColor BurnishTowardSilver(const FLinearColor& C, float W)
{
	if (W <= KINDA_SMALL_NUMBER)
	{
		return C;
	}
	const float Scale = 1.f + W * NeutralBoost;
	const FLinearColor Silver(
		FMath::Clamp(C.R * Scale, SilverFloor, SilverCeil),
		FMath::Clamp(C.G * Scale, SilverFloor, SilverCeil),
		FMath::Clamp(C.B * Scale, SilverFloor, SilverCeil),
		C.A);
	return FMath::Lerp(C, Silver, W);
}

inline FLinearColor CorrectBaseColor(const FLinearColor& C)
{
	const FLinearColor Lifted = LiftDarkPreserveHue(C);
	const float W = NeutralWeight(Saturation(Lifted));
	return BurnishTowardSilver(Lifted, W);
}

inline void Apply(FPCAppearanceSpec& Spec)
{
	const float Y = LuminanceRec709(Spec.PrimaryColor);
	Spec.RoughnessValue = RoughnessFromLuminance(Y);
	Spec.bOverrideRoughness = true;
	Spec.PrimaryColor = CorrectBaseColor(Spec.PrimaryColor);
}
} // namespace FPCMetallicColorCorrection
