// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class UButton;
class UTextBlock;
class UVerticalBox;
class UVerticalBoxSlot;
class UWidget;
class UWidgetTree;

namespace StructuralPowerIdUiHelpers {
STRUCTURALPOWER_API void SetTextStyle(UTextBlock* Text, const FString& Value, int32 Size,
                                      bool bBold = false);

STRUCTURALPOWER_API FString FormatIdOptionLabel(const FName& Id);

STRUCTURALPOWER_API UVerticalBoxSlot* AddVBoxRow(UVerticalBox* Box, UWidget* Child,
                                                 const FMargin& Padding = FMargin(0.0f, 6.0f));

STRUCTURALPOWER_API UButton* MakeLabeledButton(UWidgetTree* Tree, const TCHAR* Name,
                                               const TCHAR* Label);
}
