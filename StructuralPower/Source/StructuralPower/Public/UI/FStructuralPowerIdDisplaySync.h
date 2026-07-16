// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Routing/EStructuralChannel.h"

class UComboBoxString;
class UStructuralPowerIdConfigWidget;
class UStructuralPowerIdOptionManager;

class STRUCTURALPOWER_API FStructuralPowerIdDisplaySync {
 public:
  static bool AreFormWidgetsReady(UStructuralPowerIdConfigWidget* Widget);

  static void FlushPendingIdList(UStructuralPowerIdConfigWidget* Widget);

  static void ApplyComponentIdList(UStructuralPowerIdConfigWidget* Widget,
                                   const FStructuralComponentIdList& List);

  static void RepopulateComboFromManager(UComboBoxString* Combo,
                                         UStructuralPowerIdOptionManager* OptionManager,
                                         bool bSourceChannel);

  static void RefreshIdDisplayFromList(UStructuralPowerIdConfigWidget* Widget,
                                       const FStructuralComponentIdList& List);
};
