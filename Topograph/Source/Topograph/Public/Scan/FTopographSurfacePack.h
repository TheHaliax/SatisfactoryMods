// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

/** Bit layout for one surface sample (plan contract). */
namespace TopographSurfacePack {
/** Bit 32: water — AFGWaterVolume column (else sea-level fallback). */
inline constexpr uint64 BitSea = 1ull << 32;
inline constexpr uint64 BitBuildable = 1ull << 33;
inline constexpr uint64 ResourceIndexShift = 40;
inline constexpr uint64 ResourceIndexMask = 0xFFFFFFull;

inline uint64 PackFloatBits(float Z) {
  uint32 Bits = 0;
  FMemory::Memcpy(&Bits, &Z, sizeof(Bits));
  return static_cast<uint64>(Bits);
}

inline float UnpackFloatBits(uint64 Packed) {
  const uint32 Bits = static_cast<uint32>(Packed & 0xFFFFFFFFu);
  float Z = 0.f;
  FMemory::Memcpy(&Z, &Bits, sizeof(Z));
  return Z;
}

inline uint64 PackSurface(float HitZ, bool bSea, bool bBuildable,
                          uint32 ResourceParentIndex) {
  uint64 Out = PackFloatBits(HitZ);
  if (bSea) {
    Out |= BitSea;
  }
  if (bBuildable) {
    Out |= BitBuildable;
  }
  const uint64 Idx =
      static_cast<uint64>(ResourceParentIndex) & ResourceIndexMask;
  Out |= (Idx << ResourceIndexShift);
  return Out;
}
}  // namespace TopographSurfacePack

/** One sparse volume band hit. */
struct FTopographBandHit {
  uint8 BandIndex = 0;
  uint8 Flags = 0;
};

inline constexpr uint8 TopoBandFlagSolid = 1;
inline constexpr uint8 TopoBandFlagLandscape = 2;

/** POD sample emitted to raw tile stream. */
struct FTopographRawSample {
  uint64 Surface = 0;
  TArray<FTopographBandHit, TInlineAllocator<8>> Bands;
};

/** Raw tile file magic / version. */
inline constexpr uint32 TopographRawMagic = 0x4F504F54u;  // 'TOPO' LE
inline constexpr uint32 TopographRawVersion = 1;
