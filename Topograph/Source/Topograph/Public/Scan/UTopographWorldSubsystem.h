// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Scan/FTopographResourceSweep.h"
#include "Scan/FTopographSurfacePack.h"
#include "Scan/FTopographTileWriter.h"
#include "Scan/FTopographWaterSweep.h"
#include "Subsystems/WorldSubsystem.h"
#include "UTopographWorldSubsystem.generated.h"

class ATopographStreamingAnchor;
class UTopographSettings;

UENUM()
enum class ETopographPhase : uint8 {
  Idle,
  ProbeZ0,
  WaitStream,
  RasterTile,
  EvictTile,
  Done,
  Failed,
};

/**
 * Headless bake orchestrator: stream one macro-tile, raster, evict, repeat.
 */
UCLASS()
class TOPOGRAPH_API UTopographWorldSubsystem : public UTickableWorldSubsystem {
  GENERATED_BODY()

 public:
  virtual void Initialize(FSubsystemCollectionBase& Collection) override;
  virtual void Deinitialize() override;
  virtual void OnWorldBeginPlay(UWorld& InWorld) override;

  virtual void Tick(float DeltaTime) override;
  virtual TStatId GetStatId() const override;
  virtual bool IsTickableWhenPaused() const override { return true; }
  virtual bool IsTickableInEditor() const override { return false; }

  UFUNCTION(BlueprintCallable, Category = "Topograph")
  void StartScan();

  UFUNCTION(BlueprintCallable, Category = "Topograph")
  void StopScan();

  UFUNCTION(BlueprintCallable, Category = "Topograph")
  FString GetStatusString() const;

  bool IsScanning() const {
    return Phase != ETopographPhase::Idle && Phase != ETopographPhase::Done &&
           Phase != ETopographPhase::Failed;
  }

 private:
  void ApplyRoiFromSettings();
  void EnsureAnchor();
  void MoveAnchorToTile(int32 TileIx, int32 TileIy);
  bool IsTileCellLoaded(const FVector& Center) const;
  void BeginProbeZ0();
  void TickProbeZ0();
  void BeginRasterTile();
  void TickRasterTile();
  void FinishRasterTile();
  void AdvanceOrFinish();
  void Fail(const TCHAR* Reason);

  bool TraceColumn(float WorldX, float WorldY, FTopographRawSample& Out) const;
  bool IsLandscapeHit(const FHitResult& Hit) const;
  void RefreshVolumeFloorFromWater();

  FString MakeOutputDir() const;
  FString MakeTileRawPath(int32 TileIx, int32 TileIy) const;

  ETopographPhase Phase = ETopographPhase::Idle;

  float RoiMinX = 0.f;
  float RoiMinY = 0.f;
  float RoiMaxX = 0.f;
  float RoiMaxY = 0.f;
  float MacroTileUU = 8000.f;
  int32 TilesX = 0;
  int32 TilesY = 0;
  int32 CurTileIx = 0;
  int32 CurTileIy = 0;

  float ProbedZ0 = 0.f;
  bool bHaveZ0 = false;
  int32 ProbeSampleIndex = 0;
  int32 ProbeSamplesTotal = 0;

  float StreamWaitLeft = 0.f;

  int32 RasterX = 0;
  int32 RasterY = 0;
  uint32 TileWidth = 0;
  uint32 TileHeight = 0;
  float TileOriginX = 0.f;
  float TileOriginY = 0.f;

  TWeakObjectPtr<ATopographStreamingAnchor> Anchor;
  FTopographTileWriter TileWriter;
  FTopographResourceSweep ResourceSweep;
  FTopographWaterSweep WaterSweep;
  bool bResourcesSwept = false;

  int32 TilesCompleted = 0;
  FString LastError;
};
