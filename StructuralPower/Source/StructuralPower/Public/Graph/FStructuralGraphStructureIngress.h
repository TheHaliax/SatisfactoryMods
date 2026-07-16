// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class AFGBuildable;
struct FStructuralLightweightKey;

class STRUCTURALPOWER_API FStructuralGraphStructureIngress {
 public:
  FStructuralGraphStructureIngress() = default;

  void Bind(class FStructuralGraphSession* InSession);

  void ProcessStructure(AFGBuildable* Buildable);
  void ProcessLightweightStructure(const FStructuralLightweightKey& Key);
  void RetryAwaitingStructuralSiteEndpoints();

  /** After UF remove/split: remount endpoints whose MountParent left the graph. */
  void MarkOrphanMountEndpointsAwaiting(const TSet<int32>& AffectedRoots);
  void ReconcileAfterStructureSplit(const TSet<int32>& AffectedRoots);

 private:
  class FStructuralGraphSession* Session = nullptr;
};
