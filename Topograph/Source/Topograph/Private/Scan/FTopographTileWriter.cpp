// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Scan/FTopographTileWriter.h"

#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Serialization/Archive.h"
#include "TopographLog.h"

bool FTopographTileWriter::BeginTile(const FString& AbsolutePath, int32 TileIx,
                                     int32 TileIy, float OriginX, float OriginY,
                                     float StepUU, uint32 Width, uint32 Height,
                                     float Z0, uint32 BandCount) {
  EndTile();
  IFileManager::Get().MakeDirectory(*FPaths::GetPath(AbsolutePath), true);
  Archive.Reset(IFileManager::Get().CreateFileWriter(*AbsolutePath));
  if (!Archive.IsValid()) {
    UE_LOG(LogTopograph, Error, TEXT("Cannot open tile writer: %s"),
           *AbsolutePath);
    return false;
  }

  auto WritePod = [this](const void* Data, int64 Size) -> bool {
    Archive->Serialize(const_cast<void*>(Data), Size);
    return !Archive->IsError();
  };

  const uint32 Magic = TopographRawMagic;
  const uint32 Version = TopographRawVersion;
  if (!WritePod(&Magic, sizeof(Magic)) || !WritePod(&Version, sizeof(Version))) {
    return false;
  }
  if (!WritePod(&TileIx, sizeof(TileIx)) || !WritePod(&TileIy, sizeof(TileIy))) {
    return false;
  }
  if (!WritePod(&OriginX, sizeof(OriginX)) ||
      !WritePod(&OriginY, sizeof(OriginY)) ||
      !WritePod(&StepUU, sizeof(StepUU))) {
    return false;
  }
  if (!WritePod(&Width, sizeof(Width)) || !WritePod(&Height, sizeof(Height)) ||
      !WritePod(&Z0, sizeof(Z0)) || !WritePod(&BandCount, sizeof(BandCount))) {
    return false;
  }

  ExpectedSamples = Width * Height;
  WrittenSamples = 0;
  if (!WritePod(&ExpectedSamples, sizeof(ExpectedSamples))) {
    return false;
  }
  return true;
}

bool FTopographTileWriter::WriteSample(const FTopographRawSample& Sample) {
  if (!Archive.IsValid()) {
    return false;
  }
  Archive->Serialize(const_cast<uint64*>(&Sample.Surface),
                     sizeof(Sample.Surface));
  uint8 BandN = static_cast<uint8>(FMath::Clamp(Sample.Bands.Num(), 0, 255));
  Archive->Serialize(&BandN, sizeof(BandN));
  for (int32 i = 0; i < BandN; ++i) {
    uint8 Idx = Sample.Bands[i].BandIndex;
    uint8 Flags = Sample.Bands[i].Flags;
    Archive->Serialize(&Idx, 1);
    Archive->Serialize(&Flags, 1);
  }
  ++WrittenSamples;
  return !Archive->IsError();
}

bool FTopographTileWriter::EndTile() {
  if (!Archive.IsValid()) {
    return true;
  }
  if (WrittenSamples != ExpectedSamples) {
    UE_LOG(LogTopograph, Warning,
           TEXT("Tile sample count mismatch: wrote %u expected %u"),
           WrittenSamples, ExpectedSamples);
  }
  Archive->Flush();
  Archive.Reset();
  ExpectedSamples = 0;
  WrittenSamples = 0;
  return true;
}
