// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "UI/FStructuralPowerIdFieldMatrix.h"

#include "Blueprint/WidgetTree.h"
#include "Buildables/FGBuildable.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Config/FStructuralPowerModConfig.h"
#include "Rules/FStructuralEligibilityRules.h"
#include "StructuralPowerLog.h"
#include "UI/FStructuralPowerIdWidgetHelpers.h"
#include "UI/UStructuralPowerIdOptionManager.h"

using StructuralPowerIdUiHelpers::AddVBoxRow;
using StructuralPowerIdUiHelpers::FormatIdOptionLabel;
using StructuralPowerIdUiHelpers::MakeLabeledButton;
using StructuralPowerIdUiHelpers::SetTextStyle;

bool FStructuralPowerIdFieldMatrix::Build(
	UWidgetTree* WidgetTree,
	UVerticalBox* ContentVBox,
	AFGBuildable* TargetBuildable,
	UStructuralPowerIdOptionManager* OptionManager,
	FStructuralPowerIdFieldMatrixWidgets& OutWidgets)
{
	OutWidgets = FStructuralPowerIdFieldMatrixWidgets{};

	if (!IsValid(ContentVBox) || !IsValid(WidgetTree) || !IsValid(OptionManager)
		|| !IsValid(TargetBuildable))
	{
		return false;
	}

	const bool bLight = FStructuralEligibilityRules::IsStructuralLightConsumer(TargetBuildable);
	const bool bDual = OptionManager->NeedsDualFields();
	if (!bLight && !bDual)
	{
		return false;
	}

	UTextBlock* Subtitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Subtitle"));
	SetTextStyle(
		Subtitle,
		bDual
			? TEXT("Set source and control ids for this endpoint.")
			: TEXT("Set the source id for this endpoint."),
		13);
	AddVBoxRow(ContentVBox, Subtitle, FMargin(0.0f, 0.0f, 0.0f, 6.0f));

	UTextBlock* Hint = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HintText"));
	const bool bGroupLighting = FStructuralPowerModConfig::IsGroupLightingEnabled();
	const FString HintLine = bDual
		? FString::Printf(
			TEXT("Light source id must match panel control id. Group Lighting: %s."),
			bGroupLighting ? TEXT("ON") : TEXT("OFF (enable in mod config or !lighting)"))
		: FString::Printf(
			TEXT("Set this light's source id; match it on a panel's control id. Group Lighting: %s."),
			bGroupLighting ? TEXT("ON") : TEXT("OFF (enable in mod config or !lighting)"));
	SetTextStyle(Hint, HintLine, 11);
	AddVBoxRow(ContentVBox, Hint, FMargin(0.0f, 0.0f, 0.0f, 8.0f));

	OutWidgets.ActiveIdsText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("ActiveIdsText"));
	SetTextStyle(OutWidgets.ActiveIdsText, TEXT("Active — loading…"), 12);
	AddVBoxRow(ContentVBox, OutWidgets.ActiveIdsText, FMargin(0.0f, 0.0f, 0.0f, 12.0f));

	auto AddSuggestedSection = [&](
		const TCHAR* SectionLabel,
		const TCHAR* CustomLabel,
		const TCHAR* ComboName,
		const TCHAR* TextName,
		TObjectPtr<UComboBoxString>& OutCombo,
		TObjectPtr<UEditableTextBox>& OutText,
		int32 OptionCount,
		TFunctionRef<FName(int32)> GetOptionAt)
	{
		UTextBlock* SuggestedLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			*FString::Printf(TEXT("%sLabel"), ComboName));
		SetTextStyle(SuggestedLabel, SectionLabel, 14, true);
		AddVBoxRow(ContentVBox, SuggestedLabel, FMargin(0.0f, 8.0f, 0.0f, 4.0f));

		USizeBox* ComboHost = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(),
			*FString::Printf(TEXT("%sHost"), ComboName));
		ComboHost->SetMinDesiredHeight(36.0f);

		OutCombo = WidgetTree->ConstructWidget<UComboBoxString>(
			UComboBoxString::StaticClass(),
			ComboName);
		for (int32 Index = 0; Index < OptionCount; ++Index)
		{
			OutCombo->AddOption(FormatIdOptionLabel(GetOptionAt(Index)));
		}

		if (OptionCount > 0)
		{
			OutCombo->SetSelectedIndex(0);
		}

		ComboHost->AddChild(OutCombo);
		AddVBoxRow(ContentVBox, ComboHost, FMargin(0.0f, 0.0f, 0.0f, 8.0f));

		UTextBlock* CustomTextLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			*FString::Printf(TEXT("%sCustomLabel"), TextName));
		SetTextStyle(CustomTextLabel, CustomLabel, 13, true);
		AddVBoxRow(ContentVBox, CustomTextLabel, FMargin(0.0f, 0.0f, 0.0f, 4.0f));

		USizeBox* TextHost = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(),
			*FString::Printf(TEXT("%sHost"), TextName));
		TextHost->SetMinDesiredHeight(36.0f);

		OutText = WidgetTree->ConstructWidget<UEditableTextBox>(
			UEditableTextBox::StaticClass(),
			TextName);
		OutText->SetHintText(FText::FromString(TEXT("Enter id name")));
		TextHost->AddChild(OutText);
		AddVBoxRow(ContentVBox, TextHost, FMargin(0.0f, 0.0f, 0.0f, 12.0f));
	};

	AddSuggestedSection(
		TEXT("Suggested source id"),
		TEXT("Custom source id"),
		TEXT("SuggestedSourceCombo"),
		TEXT("AssignSourceText"),
		OutWidgets.SuggestedSourceCombo,
		OutWidgets.AssignSourceText,
		OptionManager->GetSourceOptionCount(),
		[OptionManager](int32 Index) { return OptionManager->GetSourceOptionAt(Index); });

	if (bDual)
	{
		AddSuggestedSection(
			TEXT("Suggested control id"),
			TEXT("Custom control id"),
			TEXT("SuggestedControlCombo"),
			TEXT("AssignControlText"),
			OutWidgets.SuggestedControlCombo,
			OutWidgets.AssignControlText,
			OptionManager->GetControlOptionCount(),
			[OptionManager](int32 Index) { return OptionManager->GetControlOptionAt(Index); });
	}

	UHorizontalBox* ButtonRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(),
		TEXT("ButtonRow"));

	OutWidgets.ApplyButton = MakeLabeledButton(WidgetTree, TEXT("ApplyButton"), TEXT("Apply"));
	ButtonRow->AddChild(OutWidgets.ApplyButton);
	if (UHorizontalBoxSlot* ApplySlot = Cast<UHorizontalBoxSlot>(OutWidgets.ApplyButton->Slot))
	{
		ApplySlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
		ApplySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	OutWidgets.ResetButton = MakeLabeledButton(WidgetTree, TEXT("ResetButton"), TEXT("Reset"));
	ButtonRow->AddChild(OutWidgets.ResetButton);
	if (UHorizontalBoxSlot* ResetSlot = Cast<UHorizontalBoxSlot>(OutWidgets.ResetButton->Slot))
	{
		ResetSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	AddVBoxRow(ContentVBox, ButtonRow, FMargin(0.0f, 4.0f, 0.0f, 0.0f));

	UE_LOG(LogStructuralPower, Log,
		TEXT("[HALSP] Id panel form built dual=%d sourceOpts=%d controlOpts=%d"),
		bDual ? 1 : 0,
		OptionManager->GetSourceOptionCount(),
		OptionManager->GetControlOptionCount());

	return true;
}
