// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "UI/FStructuralPowerIdDisplaySync.h"

#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Routing/EStructuralChannel.h"
#include "Routing/FStructuralPowerRouter.h"
#include "Save/AStructuralPowerGraphSubsystem.h"
#include "StructuralPowerConstants.h"
#include "StructuralPowerLog.h"
#include "UI/FStructuralPowerIdWidgetHelpers.h"
#include "UI/UStructuralPowerIdConfigWidget.h"
#include "UI/UStructuralPowerIdOptionManager.h"

using StructuralPowerIdUiHelpers::FormatIdOptionLabel;
using StructuralPowerIdUiHelpers::SetTextStyle;

bool FStructuralPowerIdDisplaySync::AreFormWidgetsReady(UStructuralPowerIdConfigWidget* Widget)
{
	if (!IsValid(Widget) || !IsValid(Widget->OptionManager) || !IsValid(Widget->TargetBuildable))
	{
		return false;
	}

	if (Widget->OptionManager->ShowsSourceIdField()
		&& (!IsValid(Widget->SuggestedSourceCombo) || !IsValid(Widget->AssignSourceText)))
	{
		return false;
	}

	if (Widget->OptionManager->ShowsControlIdField()
		&& (!IsValid(Widget->SuggestedControlCombo) || !IsValid(Widget->AssignControlText)))
	{
		return false;
	}

	return true;
}

void FStructuralPowerIdDisplaySync::FlushPendingIdList(UStructuralPowerIdConfigWidget* Widget)
{
	if (!IsValid(Widget) || !Widget->bHasPendingIdList || !IsValid(Widget->OptionManager)
		|| !AreFormWidgetsReady(Widget))
	{
		return;
	}

	Widget->OptionManager->SyncFromComponentList(Widget->PendingIdList);
	RefreshIdDisplayFromList(Widget, Widget->PendingIdList);
	Widget->bHasPendingIdList = false;
}

void FStructuralPowerIdDisplaySync::ApplyComponentIdList(
	UStructuralPowerIdConfigWidget* Widget,
	const FStructuralComponentIdList& List)
{
	if (!IsValid(Widget))
	{
		return;
	}

	Widget->PendingIdList = List;
	Widget->bHasPendingIdList = true;

	if (!IsValid(Widget->OptionManager))
	{
		UE_LOG(LogStructuralPower, Verbose,
			TEXT("[HALSP] Id panel sync deferred — option manager not ready"));
		return;
	}

	Widget->OptionManager->SyncFromComponentList(List);

	if (!AreFormWidgetsReady(Widget))
	{
		UE_LOG(LogStructuralPower, Verbose,
			TEXT("[HALSP] Id panel sync deferred — form widgets not ready"));
		return;
	}

	RefreshIdDisplayFromList(Widget, List);
	Widget->bHasPendingIdList = false;
}

void FStructuralPowerIdDisplaySync::RepopulateComboFromManager(
	UComboBoxString* Combo,
	UStructuralPowerIdOptionManager* OptionManager,
	bool bSourceChannel)
{
	if (!IsValid(Combo) || !IsValid(OptionManager))
	{
		return;
	}

	Combo->ClearOptions();
	const int32 Count = bSourceChannel
		? OptionManager->GetSourceOptionCount()
		: OptionManager->GetControlOptionCount();
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FName Id = bSourceChannel
			? OptionManager->GetSourceOptionAt(Index)
			: OptionManager->GetControlOptionAt(Index);
		Combo->AddOption(FormatIdOptionLabel(Id));
	}
}

void FStructuralPowerIdDisplaySync::RefreshIdDisplayFromList(
	UStructuralPowerIdConfigWidget* Widget,
	const FStructuralComponentIdList& List)
{
	if (!IsValid(Widget) || !IsValid(Widget->OptionManager))
	{
		return;
	}

	Widget->bSuppressServerApply = true;

	RepopulateComboFromManager(Widget->SuggestedSourceCombo, Widget->OptionManager, true);
	if (Widget->OptionManager->ShowsControlIdField())
	{
		RepopulateComboFromManager(Widget->SuggestedControlCombo, Widget->OptionManager, false);
	}

	const FName ActiveSource = List.SourceOverride.IsNone()
		? List.ResolvedSource
		: List.SourceOverride;
	const FName ActiveControl = List.ControlOverride.IsNone()
		? List.ResolvedControl
		: List.ControlOverride;

	if (IsValid(Widget->SuggestedSourceCombo))
	{
		const int32 SourceIndex = Widget->OptionManager->FindSourceOptionIndex(ActiveSource, 0);
		Widget->SuggestedSourceCombo->SetSelectedIndex(SourceIndex);
	}

	if (IsValid(Widget->SuggestedControlCombo))
	{
		const int32 ControlIndex = Widget->OptionManager->FindControlOptionIndex(ActiveControl, 0);
		Widget->SuggestedControlCombo->SetSelectedIndex(ControlIndex);
	}

	if (IsValid(Widget->AssignSourceText))
	{
		if (!List.SourceOverride.IsNone()
			&& FStructuralPowerRouter::IsPlayerChosenIdValid(List.SourceOverride))
		{
			Widget->AssignSourceText->SetText(FText::FromName(List.SourceOverride));
			Widget->AssignSourceText->SetHintText(FText::FromString(TEXT("Enter id name")));
		}
		else
		{
			Widget->AssignSourceText->SetText(FText::GetEmpty());
			const FString Hint = List.ResolvedSource.IsNone()
				? TEXT("Enter id name")
				: FString::Printf(TEXT("Auto: %s"), *List.ResolvedSource.ToString());
			Widget->AssignSourceText->SetHintText(FText::FromString(Hint));
		}
	}

	if (IsValid(Widget->AssignControlText))
	{
		if (!List.ControlOverride.IsNone()
			&& FStructuralPowerRouter::IsPlayerChosenIdValid(List.ControlOverride))
		{
			Widget->AssignControlText->SetText(FText::FromName(List.ControlOverride));
			Widget->AssignControlText->SetHintText(FText::FromString(TEXT("Enter id name")));
		}
		else
		{
			Widget->AssignControlText->SetText(FText::GetEmpty());
			const FString Hint = List.ResolvedControl.IsNone()
				|| List.ResolvedControl == StructuralPowerConstants::ControlUnconfigured
				? TEXT("Enter id name")
				: FString::Printf(TEXT("Auto: %s"), *List.ResolvedControl.ToString());
			Widget->AssignControlText->SetHintText(FText::FromString(Hint));
		}
	}

	bool bGlobalControl = false;
	if (IsValid(Widget->TargetBuildable))
	{
		if (UWorld* World = Widget->GetWorld())
		{
			if (AStructuralPowerGraphSubsystem* Graph = AStructuralPowerGraphSubsystem::Find(World))
			{
				FStructuralEndpointOverrides Overrides;
				if (Graph->GetEndpointOverrides(Widget->TargetBuildable, Overrides))
				{
					bGlobalControl = Overrides.bGlobalControl;
				}
			}
		}
	}

	if (IsValid(Widget->GlobalControlCheck))
	{
		Widget->GlobalControlCheck->SetIsChecked(bGlobalControl);
	}

	if (IsValid(Widget->ActiveIdsText))
	{
		const FString SourceLine = List.SourceOverride.IsNone()
			? (List.ResolvedSource.IsNone() ? TEXT("-") : List.ResolvedSource.ToString())
			: FString::Printf(TEXT("%s*"), *List.SourceOverride.ToString());
		FString ControlLine = List.ControlOverride.IsNone()
			? (List.ResolvedControl.IsNone() ? TEXT("-") : List.ResolvedControl.ToString())
			: FString::Printf(TEXT("%s*"), *List.ControlOverride.ToString());
		if (bGlobalControl && ControlLine != TEXT("-"))
		{
			ControlLine += TEXT(" (global)");
		}

		const bool bShowSource = Widget->OptionManager->ShowsSourceIdField();
		const bool bShowControl = Widget->OptionManager->ShowsControlIdField();
		FString Status;
		if (bShowSource && bShowControl)
		{
			Status = FString::Printf(TEXT("Active — source: %s   control: %s"), *SourceLine, *ControlLine);
		}
		else if (bShowControl)
		{
			Status = FString::Printf(TEXT("Active — control: %s"), *ControlLine);
		}
		else
		{
			Status = FString::Printf(TEXT("Active — source: %s"), *SourceLine);
		}
		SetTextStyle(Widget->ActiveIdsText, Status, 12);
	}

	Widget->bSuppressServerApply = false;

	UE_LOG(LogStructuralPower, Log,
		TEXT("[HALSP] Id panel synced ids resolvedSrc=%s resolvedCtl=%s overrideSrc=%s overrideCtl=%s"),
		List.ResolvedSource.IsNone() ? TEXT("-") : *List.ResolvedSource.ToString(),
		List.ResolvedControl.IsNone() ? TEXT("-") : *List.ResolvedControl.ToString(),
		List.SourceOverride.IsNone() ? TEXT("-") : *List.SourceOverride.ToString(),
		List.ControlOverride.IsNone() ? TEXT("-") : *List.ControlOverride.ToString());
}
