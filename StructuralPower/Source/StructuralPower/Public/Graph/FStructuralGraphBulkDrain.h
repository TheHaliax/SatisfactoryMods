// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "Core/FStructuralNodeId.h"
#include "CoreMinimal.h"

class STRUCTURALPOWER_API FStructuralGraphBulkDrain {
 public:
  FStructuralGraphBulkDrain() = default;

  void Bind(class FStructuralGraphSession* InSession);

  void FinishBulkLoadDrain();

  bool HasPendingRemesh() const {
    return bRemeshPrepared && RemeshHead < RemeshQueue.Num();
  }

  void ResetRemeshState();

 private:
  void PrepareRemeshQueue();
  bool TickRemeshBudget();
  void FinalizeAfterRemesh();

  class FStructuralGraphSession* Session = nullptr;
  TArray<FStructuralNodeId> RemeshQueue;
  TMap<int32, FStructuralNodeId> RemeshHubByRoot;
  int32 RemeshHead = 0;
  bool bRemeshPrepared = false;
};
