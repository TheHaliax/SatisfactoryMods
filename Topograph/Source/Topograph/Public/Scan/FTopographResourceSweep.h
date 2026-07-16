// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class UWorld;

struct FTopographResourceRecord {
  int32 RegistryIndex = 0;
  FString ActorName;
  FString ResourceClassName;
  int32 Purity = 0;
  FVector Location = FVector::ZeroVector;
  FRotator Rotation = FRotator::ZeroRotator;
  int32 FoundationIx = 0;
  int32 FoundationIy = 0;
};

/** Single-pass AFGResourceNode sweep → records + spatial stamp map. */
class FTopographResourceSweep {
 public:
  void Reset();
  /** Append nodes found in currently loaded actors (no clear). */
  void SweepWorldAccumulate(UWorld* World, float FoundationCellUU,
                            float RoiMinX, float RoiMinY);
  void SweepWorld(UWorld* World, float FoundationCellUU, float RoiMinX,
                  float RoiMinY);

  const TArray<FTopographResourceRecord>& GetRecords() const {
    return Records;
  }

  /** Fine-pixel key → registry index (1-based; 0 = none). */
  uint32 LookupResourceParent(float WorldX, float WorldY,
                              float SampleStepUU) const;

  bool WriteJson(const FString& AbsolutePath) const;

 private:
  TArray<FTopographResourceRecord> Records;
  TMap<uint64, uint32> PixelToRegistry;
};
