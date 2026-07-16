// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class UWorld;
class AFGWaterVolume;

/** Result of a column water-volume query. */
struct FTopographWaterColumn {
  bool bInWater = false;
  /** Approximate free-surface Z (volume bounds max). */
  float SurfaceZ = 0.f;
  /** Approximate seabed clamp Z (volume bounds min). */
  float FloorZ = 0.f;
};

/** Streamed AFGWaterVolume accumulate + XY column lookup. */
class FTopographWaterSweep {
 public:
  void Reset();

  /** Append water volumes currently loaded (dedup by actor name). */
  void SweepWorldAccumulate(UWorld* World);

  /** True if any volume recorded. */
  bool HasVolumes() const { return Records.Num() > 0; }

  int32 NumVolumes() const { return Records.Num(); }

  /**
   * Lowest volume-bound floor among recorded volumes.
   * Returns false if empty.
   */
  bool TryGetMinFloorZ(float& OutFloorZ) const;

  /** Column (X,Y) inside a water volume brush? */
  FTopographWaterColumn LookupColumn(float WorldX, float WorldY) const;

  bool WriteJson(const FString& AbsolutePath) const;

 private:
  struct FRecord {
    FString ActorName;
    FBox Bounds = FBox(ForceInit);
    TWeakObjectPtr<AFGWaterVolume> Volume;
  };

  TArray<FRecord> Records;
};
