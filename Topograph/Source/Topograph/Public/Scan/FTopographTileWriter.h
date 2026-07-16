// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Scan/FTopographSurfacePack.h"

/** Append-only little-endian raw tile writer (UE → Rust handoff). */
class FTopographTileWriter {
 public:
  bool BeginTile(const FString& AbsolutePath, int32 TileIx, int32 TileIy,
                 float OriginX, float OriginY, float StepUU, uint32 Width,
                 uint32 Height, float Z0, uint32 BandCount);

  bool WriteSample(const FTopographRawSample& Sample);
  bool EndTile();

  bool IsOpen() const { return Archive.IsValid(); }

 private:
  TUniquePtr<FArchive> Archive;
  uint32 ExpectedSamples = 0;
  uint32 WrittenSamples = 0;
};
