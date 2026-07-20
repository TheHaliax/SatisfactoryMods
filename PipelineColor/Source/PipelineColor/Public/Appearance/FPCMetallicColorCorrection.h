// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "Appearance/FPCAppearanceSpec.h"
#include "Math/Color.h"
#include "Math/UnrealMathUtility.h"

namespace FPCMetallicColorCorrection {
constexpr float AlphaMin = 0.15f;
constexpr float AlphaMax = 0.45f;
constexpr float NeutralMetallicRoughness = 1.0f;
constexpr float NeutralMatteRoughness = 4.0f;
constexpr float SatSoftLo = 0.05f;
constexpr float SatSoftHi = 0.22f;
constexpr float Burnished = 0.28f;
constexpr float Chrome = 1.0f;
constexpr float YRailLo = 0.08f;
constexpr float YRailHi = 0.50f;

inline float LuminanceRec709(const FLinearColor& C) {
  return 0.2126f * C.R + 0.7152f * C.G + 0.0722f * C.B;
}

inline float RoughnessFromLuminance(float Y) {
  const float YClamped = FMath::Clamp(Y, 0.f, 1.f);
  return AlphaMin + (AlphaMax - AlphaMin) * (YClamped * YClamped);
}

inline float Saturation(const FLinearColor& C) {
  const float CMax = FMath::Max3(C.R, C.G, C.B);
  if (CMax <= KINDA_SMALL_NUMBER) {
    return 0.f;
  }
  const float CMin = FMath::Min3(C.R, C.G, C.B);
  return (CMax - CMin) / CMax;
}

inline float NeutralWeight(float S) {
  return 1.f - FMath::SmoothStep(SatSoftLo, SatSoftHi, S);
}

inline FLinearColor SilverRail(float Y) {
  const float Span = FMath::Max(YRailHi - YRailLo, KINDA_SMALL_NUMBER);
  const float t = FMath::Clamp((Y - YRailLo) / Span, 0.f, 1.f);
  const float Shaped = FMath::SmoothStep(0.f, 1.f, t);
  const float V = FMath::Lerp(Burnished, Chrome, Shaped);
  return FLinearColor(V, V, V, 1.f);
}

inline FLinearColor CorrectBaseColor(const FLinearColor& C) {
  const float Y = LuminanceRec709(C);
  const float W = NeutralWeight(Saturation(C));
  if (W <= KINDA_SMALL_NUMBER) {
    return C;
  }
  return FMath::Lerp(C, SilverRail(Y), W);
}

inline void Apply(FPCAppearanceSpec& Spec) {
  const float Y = LuminanceRec709(Spec.PrimaryColor);
  if (Spec.CatalogKey == FName(TEXT("Neutral"))) {
    Spec.RoughnessValue = NeutralMetallicRoughness;
  } else {
    Spec.RoughnessValue = RoughnessFromLuminance(Y);
  }
  Spec.bOverrideRoughness = true;
  Spec.PrimaryColor = CorrectBaseColor(Spec.PrimaryColor);
}
}  // namespace FPCMetallicColorCorrection
