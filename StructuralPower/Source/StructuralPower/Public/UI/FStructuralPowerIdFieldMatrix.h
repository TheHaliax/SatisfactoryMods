// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class AFGBuildable;
class UButton;
class UComboBoxString;
class UEditableTextBox;
class UTextBlock;
class UStructuralPowerIdOptionManager;
class UVerticalBox;
class UWidgetTree;

/** Built id panel form widgets (source/control matrix). */
struct FStructuralPowerIdFieldMatrixWidgets
{
	TObjectPtr<UComboBoxString> SuggestedSourceCombo;
	TObjectPtr<UComboBoxString> SuggestedControlCombo;
	TObjectPtr<UEditableTextBox> AssignSourceText;
	TObjectPtr<UEditableTextBox> AssignControlText;
	TObjectPtr<UButton> ApplyButton;
	TObjectPtr<UButton> ResetButton;
	TObjectPtr<UTextBlock> ActiveIdsText;
};

/** DR-013 field matrix — builds dropdown + typed id rows into content host. */
class STRUCTURALPOWER_API FStructuralPowerIdFieldMatrix
{
public:
	static bool Build(
		UWidgetTree* WidgetTree,
		UVerticalBox* ContentVBox,
		AFGBuildable* TargetBuildable,
		UStructuralPowerIdOptionManager* OptionManager,
		FStructuralPowerIdFieldMatrixWidgets& OutWidgets);
};
