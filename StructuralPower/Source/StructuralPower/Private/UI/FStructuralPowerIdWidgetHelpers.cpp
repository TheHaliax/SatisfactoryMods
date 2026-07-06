// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "UI/FStructuralPowerIdWidgetHelpers.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "StructuralPowerConstants.h"

namespace StructuralPowerIdUiHelpers
{
void SetTextStyle(UTextBlock* Text, const FString& Value, const int32 Size, const bool bBold)
{
	if (!IsValid(Text))
	{
		return;
	}

	FSlateFontInfo Font = Text->GetFont();
	Font.Size = Size;
	if (bBold)
	{
		Font.TypefaceFontName = FName(TEXT("Bold"));
	}

	Text->SetFont(Font);
	Text->SetText(FText::FromString(Value));
	Text->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.94f, 0.96f, 1.0f)));
	Text->SetAutoWrapText(true);
}

FString FormatIdOptionLabel(const FName& Id)
{
	if (Id.IsNone())
	{
		return TEXT("(none)");
	}

	if (Id == StructuralPowerConstants::ControlBypass)
	{
		return TEXT("(bypass)");
	}

	if (Id == StructuralPowerConstants::ControlUnconfigured)
	{
		return TEXT("(unconfigured)");
	}

	return Id.ToString();
}

UVerticalBoxSlot* AddVBoxRow(
	UVerticalBox* Box,
	UWidget* Child,
	const FMargin& Padding)
{
	if (!IsValid(Box) || !IsValid(Child))
	{
		return nullptr;
	}

	Box->AddChild(Child);
	if (UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(Child->Slot))
	{
		Slot->SetPadding(Padding);
		Slot->SetHorizontalAlignment(HAlign_Fill);
	}

	return Cast<UVerticalBoxSlot>(Child->Slot);
}

UButton* MakeLabeledButton(UWidgetTree* Tree, const TCHAR* Name, const TCHAR* Label)
{
	UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
	UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		*FString::Printf(TEXT("%sLabel"), Name));
	SetTextStyle(Text, Label, 13, true);
	Button->AddChild(Text);
	return Button;
}
}
