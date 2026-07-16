// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class UPanelWidget;
class UUserWidget;
class UWidget;

class STRUCTURALPOWER_API FStructuralPowerIdWidgetTreeUtil {
 public:
  static void WalkWidgetTree(UWidget* Widget, TFunctionRef<void(UWidget*)> Visitor);

  static bool MountWidgetInHost(UWidget* Host, UWidget* Child);

  static UPanelWidget* FindWindowBodyPanel(UUserWidget* Window);
};
