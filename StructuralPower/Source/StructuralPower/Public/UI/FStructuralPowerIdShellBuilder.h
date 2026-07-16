// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class AFGBuildable;
class UBorder;
class UButton;
class UTextBlock;
class UVerticalBox;
class UUserWidget;
class UWidgetTree;
class UStructuralPowerIdConfigWidget;

struct FStructuralPowerIdShellWidgets {
  TObjectPtr<UBorder> ShellBorder;
  TObjectPtr<UTextBlock> TitleText;
  TObjectPtr<UButton> CloseButton;
  TObjectPtr<UBorder> ContentBorder;
  TObjectPtr<UVerticalBox> ContentVBox;
};

class STRUCTURALPOWER_API FStructuralPowerIdShellBuilder {
 public:
  static void ResetShellState(TObjectPtr<UWidgetTree>& WidgetTree,
                              FStructuralPowerIdShellWidgets& Shell);

  static void EnsureShellBuilt(UUserWidget* Widget, TObjectPtr<UWidgetTree>& WidgetTree,
                               FStructuralPowerIdShellWidgets& Shell);

  static void UpdateWindowTitle(const FStructuralPowerIdShellWidgets& Shell,
                                AFGBuildable* TargetBuildable);

  static void EnsureViewportPresentation(UStructuralPowerIdConfigWidget* Widget);
};
