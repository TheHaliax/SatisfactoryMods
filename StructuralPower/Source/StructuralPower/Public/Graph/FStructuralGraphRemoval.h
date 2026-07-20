// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class AFGBuildable;
struct FStructuralLightweightKey;

class STRUCTURALPOWER_API FStructuralGraphRemoval {
 public:
  FStructuralGraphRemoval() = default;

  void Bind(class FStructuralGraphSession* InSession);

  void OnBuildableRemoved(AFGBuildable* Buildable);
  void OnLightweightRemoved(const FStructuralLightweightKey& Key);

  void ScheduleStructureSplitReconcile();
  void RunStructureSplitReconcile();

 private:
  void AfterStructureNodeRemoved(const TArray<int32>& AffectedRoots, const TCHAR* Reason);

  class FStructuralGraphSession* Session = nullptr;
};
