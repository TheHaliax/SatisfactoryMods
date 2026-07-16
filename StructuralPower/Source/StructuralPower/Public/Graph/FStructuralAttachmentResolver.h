// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Graph/FStructuralOutletParentResolver.h"
#include "Lightweight/FStructuralLightweightTypes.h"

struct FStructuralComponentResolveResult {
  int32 ComponentRoot = INDEX_NONE;
  FStructuralNodeId NodeId;

  bool IsValid() const {
    return ComponentRoot != INDEX_NONE;
  }
};

class STRUCTURALPOWER_API FStructuralAttachmentResolver {
 public:
  static FStructuralWallAnchor ResolveStructuralParent(
      AFGBuildable* Buildable, UWorld* World, const FStructuralLightweightIndex& LightweightIndex);

  static FStructuralWallAnchor ResolveStructuralParent(
      AFGBuildable* Buildable, UWorld* World, const FStructuralOutletParentResolveParams& Params);

  static FStructuralComponentResolveResult ResolveStructuralComponent(
      const FStructuralConnectivityGraph& Graph, const FVector& WorldLoc, float QueryRadiusCm,
      TSubclassOf<AFGBuildable> ClassHint = nullptr);

  static int32 ResolveComponentRootForBuildable(AFGBuildable* Buildable,
                                                const FStructuralConnectivityGraph& Graph,
                                                const FStructuralLightweightIndex& LightweightIndex,
                                                UWorld* World, FStructuralNodeId& OutParentId);
};
