// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "FTopographConfig.generated.h"

/** Rocky Desert wiki grids X0Y3-X1Y4 in UU (snap to 8 m in code). */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Topograph"))
class TOPOGRAPH_API UTopographSettings : public UDeveloperSettings {
  GENERATED_BODY()

 public:
  UTopographSettings();

  virtual FName GetCategoryName() const override {
    return FName(TEXT("Plugins"));
  }

  /** Auto-start bake on dedicated server after world ready. */
  UPROPERTY(Config, EditAnywhere, Category = "Scan")
  bool bAutoStartOnDedicatedServer = true;

  /** Sample step in UU (8 m / 64 = 12.5). */
  UPROPERTY(Config, EditAnywhere, Category = "Scan",
            meta = (ClampMin = "1.0"))
  float SampleStepUU = 12.5f;

  /** Foundation cell edge in UU (800 = 8 m). */
  UPROPERTY(Config, EditAnywhere, Category = "Scan",
            meta = (ClampMin = "100.0"))
  float FoundationCellUU = 800.f;

  /** Macro-tile edge for stream+scan (foundation cells * cell size). */
  UPROPERTY(Config, EditAnywhere, Category = "Scan",
            meta = (ClampMin = "1"))
  int32 MacroTileFoundations = 10;

  /** Max band index (exclusive). Floor = SeaLevel - UnderSeaScanDepthUU. */
  UPROPERTY(Config, EditAnywhere, Category = "Scan",
            meta = (ClampMin = "1", ClampMax = "256"))
  int32 BandCount = 256;

  UPROPERTY(Config, EditAnywhere, Category = "Scan",
            meta = (ClampMin = "1.0"))
  float BandHeightUU = 400.f;

  /**
   * Fallback sea plane (UU) when no water volume covers the column.
   * Bit 32 prefers AFGWaterVolume; this is the ocean-height backup.
   */
  UPROPERTY(Config, EditAnywhere, Category = "Scan")
  float SeaLevelZUU = -24400.f;

  /** Prefer AFGWaterVolume for sea bit + volume-floor clamps. */
  UPROPERTY(Config, EditAnywhere, Category = "Scan")
  bool bUseWaterVolumes = true;

  /**
   * If no water volume on column, still set sea bit when HitZ ≤ SeaLevel
   * or WorldStatic miss.
   */
  UPROPERTY(Config, EditAnywhere, Category = "Scan")
  bool bSeaLevelFallback = true;

  /** Do not volume-scan deeper than this below water/sea floor (50 m). */
  UPROPERTY(Config, EditAnywhere, Category = "Scan",
            meta = (ClampMin = "0.0"))
  float UnderSeaScanDepthUU = 5000.f;

  /**
   * Stop extending volume scan in a direction after this much consecutive
   * clear air (256 m default).
   */
  UPROPERTY(Config, EditAnywhere, Category = "Scan",
            meta = (ClampMin = "400.0"))
  float ClearAirStopUU = 25600.f;

  /** Skip volume bands when surface WorldStatic miss (void/ocean). */
  UPROPERTY(Config, EditAnywhere, Category = "Scan")
  bool bSkipVolumeOnMiss = true;

  UPROPERTY(Config, EditAnywhere, Category = "Scan")
  float TraceStartZUU = 100000.f;

  UPROPERTY(Config, EditAnywhere, Category = "Scan")
  float TraceEndZUU = -50000.f;

  /** ROI min (inclusive). Rocky Desert default. */
  UPROPERTY(Config, EditAnywhere, Category = "ROI")
  FVector2D RoiMinXY = FVector2D(-324600.f, -67800.f);

  /** ROI max (exclusive after snap). Rocky Desert default. */
  UPROPERTY(Config, EditAnywhere, Category = "ROI")
  FVector2D RoiMaxXY = FVector2D(-119800.f, 137000.f);

  /** Use full world AABB instead of Rocky Desert ROI. */
  UPROPERTY(Config, EditAnywhere, Category = "ROI")
  bool bFullWorldAabb = false;

  UPROPERTY(Config, EditAnywhere, Category = "ROI")
  FVector2D WorldMinXY = FVector2D(-324600.f, -375000.f);

  UPROPERTY(Config, EditAnywhere, Category = "ROI")
  FVector2D WorldMaxXY = FVector2D(425300.f, 375000.f);

  /** Samples attempted per tick (game-thread budget). */
  UPROPERTY(Config, EditAnywhere, Category = "Performance",
            meta = (ClampMin = "1"))
  int32 SamplesPerTick = 256;

  /** WP streaming source radius (UU). Halo around macro-tile. */
  UPROPERTY(Config, EditAnywhere, Category = "Streaming",
            meta = (ClampMin = "1000.0"))
  float StreamingRadiusUU = 16000.f;

  /** FG spatial hash grid name (empty = try MainGrid then first). */
  UPROPERTY(Config, EditAnywhere, Category = "Streaming")
  FName WpGridName = NAME_None;

  /** Output folder under ProjectSavedDir (or absolute). */
  UPROPERTY(Config, EditAnywhere, Category = "Output")
  FString OutputSubdir = TEXT("Topograph/rocky_desert");

  /** Wait seconds after moving streaming source before raster. */
  UPROPERTY(Config, EditAnywhere, Category = "Streaming",
            meta = (ClampMin = "0.0"))
  float StreamSettleSeconds = 2.f;
};
