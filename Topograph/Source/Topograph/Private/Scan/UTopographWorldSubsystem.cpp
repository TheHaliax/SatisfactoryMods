// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Scan/UTopographWorldSubsystem.h"

#include "Config/FTopographConfig.h"
#include "Engine/World.h"
#include "FGWorldPartitionRuntimeSpatialHash.h"
#include "HAL/FileManager.h"
#include "LandscapeProxy.h"
#include "Misc/Paths.h"
#include "Scan/ATopographStreamingAnchor.h"
#include "TopographLog.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionSubsystem.h"

void UTopographWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection) {
  Super::Initialize(Collection);
}

void UTopographWorldSubsystem::Deinitialize() {
  StopScan();
  if (ATopographStreamingAnchor* A = Anchor.Get()) {
    A->Destroy();
  }
  Anchor.Reset();
  Super::Deinitialize();
}

void UTopographWorldSubsystem::OnWorldBeginPlay(UWorld& InWorld) {
  Super::OnWorldBeginPlay(InWorld);
  const UTopographSettings* Settings = GetDefault<UTopographSettings>();
  if (!Settings) {
    return;
  }
  if (Settings->bAutoStartOnDedicatedServer &&
      InWorld.GetNetMode() == NM_DedicatedServer) {
    UE_LOG(LogTopograph, Log,
           TEXT("Auto-start bake on dedicated server"));
    StartScan();
  }
}

TStatId UTopographWorldSubsystem::GetStatId() const {
  RETURN_QUICK_DECLARE_CYCLE_STAT(UTopographWorldSubsystem,
                                  STATGROUP_Tickables);
}

void UTopographWorldSubsystem::ApplyRoiFromSettings() {
  const UTopographSettings* S = GetDefault<UTopographSettings>();
  check(S);
  const float Cell = S->FoundationCellUU;
  FVector2D Min = S->bFullWorldAabb ? S->WorldMinXY : S->RoiMinXY;
  FVector2D Max = S->bFullWorldAabb ? S->WorldMaxXY : S->RoiMaxXY;
  RoiMinX = FMath::FloorToFloat(Min.X / Cell) * Cell;
  RoiMinY = FMath::FloorToFloat(Min.Y / Cell) * Cell;
  RoiMaxX = FMath::CeilToFloat(Max.X / Cell) * Cell;
  RoiMaxY = FMath::CeilToFloat(Max.Y / Cell) * Cell;
  MacroTileUU = Cell * static_cast<float>(S->MacroTileFoundations);
  const float SpanX = RoiMaxX - RoiMinX;
  const float SpanY = RoiMaxY - RoiMinY;
  TilesX = FMath::Max(1, FMath::CeilToInt(SpanX / MacroTileUU));
  TilesY = FMath::Max(1, FMath::CeilToInt(SpanY / MacroTileUU));
}

void UTopographWorldSubsystem::EnsureAnchor() {
  if (Anchor.IsValid()) {
    return;
  }
  UWorld* World = GetWorld();
  if (!World) {
    return;
  }
  FActorSpawnParameters Params;
  Params.SpawnCollisionHandlingOverride =
      ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  Params.ObjectFlags |= RF_Transient;
  ATopographStreamingAnchor* Spawned =
      World->SpawnActor<ATopographStreamingAnchor>(
          ATopographStreamingAnchor::StaticClass(), FVector::ZeroVector,
          FRotator::ZeroRotator, Params);
  Anchor = Spawned;
}

void UTopographWorldSubsystem::MoveAnchorToTile(int32 TileIx, int32 TileIy) {
  EnsureAnchor();
  const UTopographSettings* S = GetDefault<UTopographSettings>();
  if (!S || !Anchor.IsValid()) {
    return;
  }
  const float Ox = RoiMinX + static_cast<float>(TileIx) * MacroTileUU;
  const float Oy = RoiMinY + static_cast<float>(TileIy) * MacroTileUU;
  const float Cx = Ox + MacroTileUU * 0.5f;
  const float Cy = Oy + MacroTileUU * 0.5f;
  const float Radius =
      FMath::Max(S->StreamingRadiusUU, MacroTileUU * 0.75f);
  Anchor->ConfigureSource(FVector(Cx, Cy, 0.f), Radius);
  UE_LOG(LogTopograph, Log,
         TEXT("Streaming anchor → tile (%d,%d) center=(%.0f,%.0f) r=%.0f"),
         TileIx, TileIy, Cx, Cy, Radius);
}

bool UTopographWorldSubsystem::IsTileCellLoaded(const FVector& Center) const {
  UWorld* World = GetWorld();
  if (!World) {
    return false;
  }
  if (ATopographStreamingAnchor* A = Anchor.Get()) {
    if (A->StreamingSource && A->StreamingSource->IsStreamingCompleted()) {
      return true;
    }
  }
  UWorldPartition* WP = World->GetWorldPartition();
  if (!WP) {
    // Non-WP worlds: treat as loaded.
    return true;
  }
  UFGWorldPartitionRuntimeSpatialHash* FgHash =
      Cast<UFGWorldPartitionRuntimeSpatialHash>(WP->RuntimeHash);
  const UTopographSettings* S = GetDefault<UTopographSettings>();
  FName GridName = S ? S->WpGridName : NAME_None;
  if (FgHash) {
    if (GridName.IsNone()) {
      GridName = FName(TEXT("MainGrid"));
    }
    if (FgHash->IsCellContainingWorldLocationLoaded(GridName, Center)) {
      return true;
    }
  }
  if (UWorldPartitionSubsystem* Wps =
          World->GetSubsystem<UWorldPartitionSubsystem>()) {
    if (Wps->IsStreamingCompleted()) {
      return true;
    }
  }
  return false;
}

FString UTopographWorldSubsystem::MakeOutputDir() const {
  const UTopographSettings* S = GetDefault<UTopographSettings>();
  FString Sub = S ? S->OutputSubdir : TEXT("Topograph");
  if (FPaths::IsRelative(Sub)) {
    return FPaths::Combine(FPaths::ProjectSavedDir(), Sub);
  }
  return Sub;
}

FString UTopographWorldSubsystem::MakeTileRawPath(int32 TileIx,
                                                  int32 TileIy) const {
  return FPaths::Combine(
      MakeOutputDir(),
      FString::Printf(TEXT("tile_%04d_%04d.raw"), TileIx, TileIy));
}

void UTopographWorldSubsystem::StartScan() {
  if (IsScanning()) {
    UE_LOG(LogTopograph, Warning, TEXT("Scan already running"));
    return;
  }
  ApplyRoiFromSettings();
  CurTileIx = 0;
  CurTileIy = 0;
  TilesCompleted = 0;
  bHaveZ0 = false;
  bResourcesSwept = false;
  ResourceSweep.Reset();
  WaterSweep.Reset();
  LastError.Reset();
  EnsureAnchor();
  IFileManager::Get().MakeDirectory(*MakeOutputDir(), true);
  UE_LOG(LogTopograph, Log,
         TEXT("Scan start ROI=[%.0f,%.0f]–[%.0f,%.0f] tiles=%dx%d macro=%.0f"),
         RoiMinX, RoiMinY, RoiMaxX, RoiMaxY, TilesX, TilesY, MacroTileUU);
  BeginProbeZ0();
}

void UTopographWorldSubsystem::StopScan() {
  TileWriter.EndTile();
  if (ATopographStreamingAnchor* A = Anchor.Get()) {
    if (A->StreamingSource) {
      A->StreamingSource->DisableStreamingSource();
    }
  }
  Phase = ETopographPhase::Idle;
}

FString UTopographWorldSubsystem::GetStatusString() const {
  return FString::Printf(
      TEXT("phase=%d tile=(%d,%d)/%dx%d done=%d z0=%.1f err=%s"),
      static_cast<int32>(Phase), CurTileIx, CurTileIy, TilesX, TilesY,
      TilesCompleted, ProbedZ0, *LastError);
}

void UTopographWorldSubsystem::Fail(const TCHAR* Reason) {
  LastError = Reason;
  Phase = ETopographPhase::Failed;
  UE_LOG(LogTopograph, Error, TEXT("Scan failed: %s"), Reason);
  StopScan();
  Phase = ETopographPhase::Failed;
}

void UTopographWorldSubsystem::RefreshVolumeFloorFromWater() {
  const UTopographSettings* S = GetDefault<UTopographSettings>();
  if (!S) {
    return;
  }
  // Baseline: sea plane - under-sea depth.
  float Floor = S->SeaLevelZUU - S->UnderSeaScanDepthUU;
  if (S->bUseWaterVolumes) {
    float WaterFloor = 0.f;
    if (WaterSweep.TryGetMinFloorZ(WaterFloor)) {
      Floor = FMath::Min(Floor, WaterFloor - S->UnderSeaScanDepthUU);
    }
  }
  ProbedZ0 = Floor;
  bHaveZ0 = true;
}

void UTopographWorldSubsystem::BeginProbeZ0() {
  const UTopographSettings* S = GetDefault<UTopographSettings>();
  RefreshVolumeFloorFromWater();
  UE_LOG(LogTopograph, Log,
         TEXT("Volume floor Z0=%.1f (sea=%.1f under=%.1f clearAir=%.1f "
              "waterVols=%d)"),
         ProbedZ0, S->SeaLevelZUU, S->UnderSeaScanDepthUU, S->ClearAirStopUU,
         WaterSweep.NumVolumes());
  CurTileIx = 0;
  CurTileIy = 0;
  MoveAnchorToTile(CurTileIx, CurTileIy);
  StreamWaitLeft = S->StreamSettleSeconds;
  Phase = ETopographPhase::WaitStream;
  ProbeSampleIndex = -2;  // begin raster after wait
}

void UTopographWorldSubsystem::TickProbeZ0() {
  // Unused — volume floor is fixed from sea level.
}

void UTopographWorldSubsystem::BeginRasterTile() {
  const UTopographSettings* S = GetDefault<UTopographSettings>();
  if (!S || !bHaveZ0) {
    Fail(TEXT("Raster without Z0"));
    return;
  }
  TileOriginX = RoiMinX + static_cast<float>(CurTileIx) * MacroTileUU;
  TileOriginY = RoiMinY + static_cast<float>(CurTileIy) * MacroTileUU;
  const float TileMaxX = FMath::Min(TileOriginX + MacroTileUU, RoiMaxX);
  const float TileMaxY = FMath::Min(TileOriginY + MacroTileUU, RoiMaxY);
  TileWidth = static_cast<uint32>(
      FMath::Max(1, FMath::FloorToInt((TileMaxX - TileOriginX) / S->SampleStepUU)));
  TileHeight = static_cast<uint32>(
      FMath::Max(1, FMath::FloorToInt((TileMaxY - TileOriginY) / S->SampleStepUU)));
  RasterX = 0;
  RasterY = 0;
  ResourceSweep.SweepWorldAccumulate(GetWorld(), S->FoundationCellUU, RoiMinX,
                                     RoiMinY);
  if (S->bUseWaterVolumes) {
    WaterSweep.SweepWorldAccumulate(GetWorld());
    RefreshVolumeFloorFromWater();
  }
  if (!TileWriter.BeginTile(MakeTileRawPath(CurTileIx, CurTileIy), CurTileIx,
                            CurTileIy, TileOriginX, TileOriginY, S->SampleStepUU,
                            TileWidth, TileHeight, ProbedZ0,
                            static_cast<uint32>(S->BandCount))) {
    Fail(TEXT("BeginTile failed"));
    return;
  }
  Phase = ETopographPhase::RasterTile;
  UE_LOG(LogTopograph, Log,
         TEXT("Raster tile (%d,%d) %ux%u z0=%.1f waterVols=%d"), CurTileIx,
         CurTileIy, TileWidth, TileHeight, ProbedZ0, WaterSweep.NumVolumes());
}

bool UTopographWorldSubsystem::IsLandscapeHit(const FHitResult& Hit) const {
  AActor* Actor = Hit.GetActor();
  if (!Actor) {
    return false;
  }
  if (Actor->IsA(ALandscapeProxy::StaticClass())) {
    return true;
  }
  const FString Name = Actor->GetClass()->GetName();
  return Name.Contains(TEXT("Landscape"));
}

bool UTopographWorldSubsystem::TraceColumn(float WorldX, float WorldY,
                                           FTopographRawSample& Out) const {
  Out = FTopographRawSample{};
  UWorld* World = GetWorld();
  const UTopographSettings* S = GetDefault<UTopographSettings>();
  if (!World || !S) {
    return false;
  }

  FTopographWaterColumn Water;
  if (S->bUseWaterVolumes) {
    Water = WaterSweep.LookupColumn(WorldX, WorldY);
  }

  FHitResult Hit;
  const FVector Start(WorldX, WorldY, S->TraceStartZUU);
  const FVector End(WorldX, WorldY, S->TraceEndZUU);
  FCollisionQueryParams Params(SCENE_QUERY_STAT(TopographCol), false);
  const bool bHit = World->LineTraceSingleByChannel(Hit, Start, End,
                                                    ECC_WorldStatic, Params);
  float HitZ = S->TraceEndZUU;
  bool bBuildable = false;
  if (bHit) {
    HitZ = Hit.ImpactPoint.Z;
    bBuildable = IsLandscapeHit(Hit);
  } else if (Water.bInWater) {
    // Ocean / lake with no WorldStatic hit — surface = water free surface.
    HitZ = Water.SurfaceZ;
  }

  bool bSea = Water.bInWater;
  if (!bSea && S->bSeaLevelFallback) {
    bSea = !bHit || HitZ <= S->SeaLevelZUU;
  }

  const uint32 ResParent =
      ResourceSweep.LookupResourceParent(WorldX, WorldY, S->SampleStepUU);
  Out.Surface =
      TopographSurfacePack::PackSurface(HitZ, bSea, bBuildable, ResParent);

  // True void (no solid, no water): skip volume. Water columns still probe
  // for seabed down to tile Z0 (clamped from water volume floors).
  if (!bHit && !Water.bInWater && S->bSkipVolumeOnMiss) {
    return true;
  }

  const int32 BandMax = FMath::Clamp(S->BandCount, 1, 256);
  const float BandH = FMath::Max(1.f, S->BandHeightUU);
  const int32 ClearBands = FMath::Max(
      1, FMath::CeilToInt(S->ClearAirStopUU / BandH));

  auto BandSolid = [&](int32 Bi, bool& bLandscape) -> bool {
    bLandscape = false;
    if (Bi < 0 || Bi >= BandMax) {
      return false;
    }
    const float ZLo = ProbedZ0 + static_cast<float>(Bi) * BandH;
    const float ZHi = ZLo + BandH;
    FHitResult BandHit;
    if (!World->LineTraceSingleByChannel(BandHit, FVector(WorldX, WorldY, ZHi),
                                         FVector(WorldX, WorldY, ZLo),
                                         ECC_WorldStatic, Params)) {
      return false;
    }
    bLandscape = IsLandscapeHit(BandHit);
    return true;
  };

  auto EmitBand = [&](int32 Bi, bool bLandscape) {
    FTopographBandHit B;
    B.BandIndex = static_cast<uint8>(Bi);
    B.Flags = TopoBandFlagSolid;
    if (bLandscape) {
      B.Flags |= TopoBandFlagLandscape;
    }
    Out.Bands.Add(B);
  };

  int32 HitBand = FMath::FloorToInt((HitZ - ProbedZ0) / BandH);
  HitBand = FMath::Clamp(HitBand, 0, BandMax - 1);

  // Walk up from surface until ClearAirStop of consecutive empty.
  {
    int32 EmptyRun = 0;
    for (int32 Bi = HitBand; Bi < BandMax; ++Bi) {
      bool bLand = false;
      if (BandSolid(Bi, bLand)) {
        EmitBand(Bi, bLand);
        EmptyRun = 0;
      } else {
        ++EmptyRun;
        if (EmptyRun >= ClearBands) {
          break;
        }
      }
    }
  }

  // Walk down from just below surface until clear-air stop or floor.
  {
    int32 EmptyRun = 0;
    for (int32 Bi = HitBand - 1; Bi >= 0; --Bi) {
      bool bLand = false;
      if (BandSolid(Bi, bLand)) {
        EmitBand(Bi, bLand);
        EmptyRun = 0;
      } else {
        ++EmptyRun;
        if (EmptyRun >= ClearBands) {
          break;
        }
      }
    }
  }
  return true;
}

void UTopographWorldSubsystem::TickRasterTile() {
  const UTopographSettings* S = GetDefault<UTopographSettings>();
  if (!S) {
    Fail(TEXT("No settings"));
    return;
  }
  const int32 Budget = S->SamplesPerTick;
  for (int32 n = 0; n < Budget; ++n) {
    if (RasterY >= static_cast<int32>(TileHeight)) {
      FinishRasterTile();
      return;
    }
    const float X =
        TileOriginX + (static_cast<float>(RasterX) + 0.5f) * S->SampleStepUU;
    const float Y =
        TileOriginY + (static_cast<float>(RasterY) + 0.5f) * S->SampleStepUU;
    FTopographRawSample Sample;
    TraceColumn(X, Y, Sample);
    if (!TileWriter.WriteSample(Sample)) {
      Fail(TEXT("WriteSample failed"));
      return;
    }
    ++RasterX;
    if (RasterX >= static_cast<int32>(TileWidth)) {
      RasterX = 0;
      ++RasterY;
    }
  }
}

void UTopographWorldSubsystem::FinishRasterTile() {
  TileWriter.EndTile();
  ++TilesCompleted;
  Phase = ETopographPhase::EvictTile;
  if (ATopographStreamingAnchor* A = Anchor.Get()) {
    if (A->StreamingSource) {
      A->StreamingSource->DisableStreamingSource();
    }
  }
  UE_LOG(LogTopograph, Log, TEXT("Finished tile (%d,%d) total=%d"), CurTileIx,
         CurTileIy, TilesCompleted);
  AdvanceOrFinish();
}

void UTopographWorldSubsystem::AdvanceOrFinish() {
  ++CurTileIx;
  if (CurTileIx >= TilesX) {
    CurTileIx = 0;
    ++CurTileIy;
  }
  if (CurTileIy >= TilesY) {
    ResourceSweep.WriteJson(
        FPaths::Combine(MakeOutputDir(), TEXT("resources.json")));
    WaterSweep.WriteJson(
        FPaths::Combine(MakeOutputDir(), TEXT("waters.json")));
    bResourcesSwept = true;
    Phase = ETopographPhase::Done;
    UE_LOG(LogTopograph, Log,
           TEXT("Scan complete. tiles=%d waterVols=%d out=%s"), TilesCompleted,
           WaterSweep.NumVolumes(), *MakeOutputDir());
    return;
  }
  MoveAnchorToTile(CurTileIx, CurTileIy);
  StreamWaitLeft = GetDefault<UTopographSettings>()->StreamSettleSeconds;
  Phase = ETopographPhase::WaitStream;
  ProbeSampleIndex = -2;
}

void UTopographWorldSubsystem::Tick(float DeltaTime) {
  switch (Phase) {
    case ETopographPhase::WaitStream: {
      StreamWaitLeft -= DeltaTime;
      const FVector Center =
          Anchor.IsValid() ? Anchor->GetActorLocation()
                           : FVector(RoiMinX, RoiMinY, 0.f);
      if (StreamWaitLeft <= 0.f && IsTileCellLoaded(Center)) {
        if (ProbeSampleIndex == -1) {
          ProbeSampleIndex = 0;
          Phase = ETopographPhase::ProbeZ0;
        } else if (ProbeSampleIndex == -2) {
          BeginRasterTile();
        } else {
          Phase = ETopographPhase::ProbeZ0;
        }
      } else if (StreamWaitLeft <= -30.f) {
        // Soft continue after timeout — spike may not see FG cell API.
        UE_LOG(LogTopograph, Warning,
               TEXT("Stream wait timeout; continuing anyway"));
        StreamWaitLeft = 0.f;
        if (ProbeSampleIndex == -1) {
          ProbeSampleIndex = 0;
          Phase = ETopographPhase::ProbeZ0;
        } else {
          BeginRasterTile();
        }
      }
      break;
    }
    case ETopographPhase::ProbeZ0:
      TickProbeZ0();
      break;
    case ETopographPhase::RasterTile:
      TickRasterTile();
      break;
    default:
      break;
  }
}
