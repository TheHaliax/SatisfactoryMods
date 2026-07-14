// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class AFGBuildable;
class UButton;
class UCheckBox;
class UComboBoxString;
class UEditableTextBox;
class UTextBlock;
class UStructuralPowerIdOptionManager;
class UVerticalBox;
class UWidgetTree;

struct FStructuralPowerIdFieldMatrixWidgets
{
	TObjectPtr<UComboBoxString> SuggestedSourceCombo;
	TObjectPtr<UComboBoxString> SuggestedControlCombo;
	TObjectPtr<UEditableTextBox> AssignSourceText;
	TObjectPtr<UEditableTextBox> AssignControlText;
	TObjectPtr<UCheckBox> GlobalControlCheck;
	TObjectPtr<UButton> ApplyButton;
	TObjectPtr<UButton> ResetButton;
	TObjectPtr<UTextBlock> ActiveIdsText;
};

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
