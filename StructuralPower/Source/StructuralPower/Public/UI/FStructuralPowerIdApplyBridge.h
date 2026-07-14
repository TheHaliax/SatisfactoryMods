// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class AFGBuildable;
class AFGPlayerController;
class UCheckBox;
class UComboBoxString;
class UEditableTextBox;
class UStructuralPowerIdOptionManager;

class STRUCTURALPOWER_API FStructuralPowerIdApplyBridge
{
public:
	static void RequestComponentIdList(AFGPlayerController* PlayerController, AFGBuildable* Target);

	static void ApplyEndpointIds(
		AFGPlayerController* PlayerController,
		AFGBuildable* Target,
		FName Source,
		FName Control,
		bool bClearSource,
		bool bClearControl);

	static void ApplySourceIndex(
		AFGPlayerController* PlayerController,
		AFGBuildable* Target,
		UStructuralPowerIdOptionManager* OptionManager,
		int32 Index);

	static void ApplyControlIndex(
		AFGPlayerController* PlayerController,
		AFGBuildable* Target,
		UStructuralPowerIdOptionManager* OptionManager,
		int32 Index);

	static void ApplyTypedIds(
		AFGPlayerController* PlayerController,
		AFGBuildable* Target,
		UStructuralPowerIdOptionManager* OptionManager,
		const UComboBoxString* SuggestedSourceCombo,
		const UComboBoxString* SuggestedControlCombo,
		const UEditableTextBox* AssignSourceText,
		const UEditableTextBox* AssignControlText,
		const UCheckBox* GlobalControlCheck = nullptr);

	static void ResetEndpointIds(
		AFGPlayerController* PlayerController,
		AFGBuildable* Target,
		bool bClearSource,
		bool bClearControl);
};
