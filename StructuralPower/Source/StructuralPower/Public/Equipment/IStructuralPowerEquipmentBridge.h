// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class FStructuralGraphSession;
struct FStructuralHoverpackAnchorQuery;

class STRUCTURALPOWER_API IStructuralPowerEquipmentBridge {
 public:
  virtual ~IStructuralPowerEquipmentBridge() = default;

  virtual FName GetKind() const = 0;

  virtual void RegisterHooks() {
  }

  virtual bool QueryHoverpackStructuralAnchor(FStructuralGraphSession& Session,
                                              const FVector& QueryLoc, float MaxHorizontal,
                                              float MaxVertical,
                                              FStructuralHoverpackAnchorQuery& Out) const {
    return false;
  }
};
