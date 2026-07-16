// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class AFGHoverPack;
class AFGBuildableRailroadTrack;
class UFGPowerConnectionComponent;

enum class EStructuralHoverpackTetherSource : uint8 {
  None,
  VisibleConnector,
  StructuralGeometry,
};

struct FStructuralHoverpackSession {
  EStructuralHoverpackTetherSource Source = EStructuralHoverpackTetherSource::None;
  TWeakObjectPtr<UFGPowerConnectionComponent> Feed;
  FVector Anchor = FVector::ZeroVector;
  int32 StructuralRoot = INDEX_NONE;
  float MaxHorizontal = 0.0f;
  float MaxVertical = 0.0f;
  float BaseSearchRadius = 0.0f;
  float DisconnectionTimer = 0.0f;
  float ResolveMissTimer = 0.0f;
  float SearchTimer = 0.0f;
  double SuppressReconnectUntil = 0.0;
  bool bClientTetherPublished = false;
  FVector ClientPublishedAnchor = FVector::ZeroVector;
  float ClientPublishedMaxHorizontal = 0.0f;
  float ClientPublishedMaxVertical = 0.0f;
};

struct FStructuralHoverpackAxisLimits {
  float Horizontal = 0.0f;
  float Vertical = 0.0f;
};

struct FStructuralHoverpackTetherMetrics {
  float Horizontal = 0.0f;
  float Vertical = 0.0f;
  float Limiting = 0.0f;
  float StraightLine = 0.0f;
};

class STRUCTURALPOWER_API FStructuralHoverpackBridge {
 public:
  static void RegisterHooks();

  static void ApplyClientTetherMirror(const FVector& Anchor, float MaxHorizontal,
                                      float MaxVertical);
  static void ClearClientTetherMirror();

 private:
  static bool OwnsConnectivity(const AFGHoverPack* Pack);
  static const FStructuralHoverpackSession* FindSession(const AFGHoverPack* Pack);
  static float GetVanillaFieldRadius(const FStructuralHoverpackSession& Session);
  static bool TryGetClientTetherMirror(FVector& OutAnchor, float& OutMaxHorizontal,
                                       float& OutMaxVertical);

  static void BeforeTick(AFGHoverPack* Pack, float DeltaTime);
  static void AfterTick(AFGHoverPack* Pack, float DeltaTime);
  static void OnUnEquip(AFGHoverPack* Pack);

  static FStructuralHoverpackSession& GetOrCreateSession(AFGHoverPack* Pack);
  static void ClearSession(AFGHoverPack* Pack, bool bDisconnectPower, const TCHAR* Event);

  static bool IsStructuralPathEnabled(const AFGHoverPack* Pack);
  static float GetBaseSearchRadius(const AFGHoverPack* Pack);
  static FStructuralHoverpackAxisLimits GetSessionAxisLimits(
      EStructuralHoverpackTetherSource Source, float BaseSearchRadius);
  static float GetReconnectCooldownSeconds(const AFGHoverPack* Pack);
  static FVector GetOwnerLocation(const AFGHoverPack* Pack);

  static bool FindNearestVisibleConnector(AFGHoverPack* Pack, float MaxDistance,
                                          UFGPowerConnectionComponent* PreferredFeed,
                                          UFGPowerConnectionComponent*& OutFeed, FVector& OutAnchor,
                                          float& OutDistance);
  static bool FindStructuralTether(AFGHoverPack* Pack, float MaxHorizontal, float MaxVertical,
                                   UFGPowerConnectionComponent*& OutFeed, FVector& OutAnchor,
                                   int32& OutRoot, float& OutDistance);
  static bool ResolveTether(AFGHoverPack* Pack);
  static bool ValidateAndRefreshSession(AFGHoverPack* Pack, FStructuralHoverpackSession& Session);
  static bool ApplySession(AFGHoverPack* Pack, FStructuralHoverpackSession& Session,
                           EStructuralHoverpackTetherSource Source,
                           UFGPowerConnectionComponent* Feed, const FVector& Anchor,
                           int32 StructuralRoot, float Distance, bool bLogTransition);
  static void MaintainSession(AFGHoverPack* Pack, float DeltaTime);
  static void SyncReplicationFields(AFGHoverPack* Pack, FStructuralHoverpackSession& Session);
  static void SyncStructuralVirtualTether(AFGHoverPack* Pack, FStructuralHoverpackSession& Session);
  static void PublishStructuralTetherToOwningClient(AFGHoverPack* Pack,
                                                    FStructuralHoverpackSession& Session,
                                                    bool bClear);
  static void DisconnectPower(AFGHoverPack* Pack);
  static bool SyncPowerFeed(AFGHoverPack* Pack, UFGPowerConnectionComponent* Feed);
  static void RestoreAuthoritativeState(AFGHoverPack* Pack);
  static bool ShouldPublishSessionToVanilla(const AFGHoverPack* Pack,
                                            const FStructuralHoverpackSession& Session);
  static bool ResolvePublishedTether(const AFGHoverPack* Pack, FVector& OutAnchor,
                                     float& OutMaxHorizontal, float& OutMaxVertical,
                                     FStructuralHoverpackTetherMetrics& OutMetrics);
  static void LogTransition(const AFGHoverPack* Pack, const TCHAR* Event,
                            const FStructuralHoverpackSession& Session, float Distance);

  static TMap<TWeakObjectPtr<AFGHoverPack>, FStructuralHoverpackSession> Sessions;
};
