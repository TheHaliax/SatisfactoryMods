// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "UI/FStructuralPowerIdWidgetTreeUtil.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/NamedSlot.h"
#include "Components/PanelWidget.h"

void FStructuralPowerIdWidgetTreeUtil::WalkWidgetTree(
	UWidget* Widget,
	TFunctionRef<void(UWidget*)> Visitor)
{
	if (!IsValid(Widget))
	{
		return;
	}

	Visitor(Widget);

	if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
	{
		const int32 Count = Panel->GetChildrenCount();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			WalkWidgetTree(Panel->GetChildAt(Index), Visitor);
		}
	}
}

bool FStructuralPowerIdWidgetTreeUtil::MountWidgetInHost(UWidget* Host, UWidget* Child)
{
	if (!IsValid(Host) || !IsValid(Child))
	{
		return false;
	}

	if (UNamedSlot* NamedSlot = Cast<UNamedSlot>(Host))
	{
		NamedSlot->SetContent(Child);
		return true;
	}

	if (UPanelWidget* Panel = Cast<UPanelWidget>(Host))
	{
		Panel->AddChild(Child);
		return true;
	}

	return false;
}

UPanelWidget* FStructuralPowerIdWidgetTreeUtil::FindWindowBodyPanel(UUserWidget* Window)
{
	if (!IsValid(Window) || !IsValid(Window->WidgetTree))
	{
		return nullptr;
	}

	UPanelWidget* NamedSlotBody = nullptr;
	UPanelWidget* NamedPanelBody = nullptr;
	WalkWidgetTree(Window->WidgetTree->RootWidget, [&](UWidget* Widget)
	{
		const FString Name = Widget->GetName();
		const bool bNameMatch = Name.Contains(TEXT("WindowBody"), ESearchCase::IgnoreCase)
			|| Name.Contains(TEXT("Window_Body"), ESearchCase::IgnoreCase)
			|| Name.Contains(TEXT("WindowContent"), ESearchCase::IgnoreCase)
			|| Name.Equals(TEXT("Body"), ESearchCase::IgnoreCase);

		if (UNamedSlot* NamedSlot = Cast<UNamedSlot>(Widget))
		{
			if (bNameMatch)
			{
				NamedSlotBody = NamedSlot;
			}
		}

		if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
		{
			if (bNameMatch)
			{
				NamedPanelBody = Panel;
			}
		}
	});

	return NamedSlotBody ? NamedSlotBody : NamedPanelBody;
}
