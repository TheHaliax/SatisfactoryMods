// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "UI/FStructuralPowerIdApplyBridge.h"

#include "Buildables/FGBuildable.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "FGPlayerController.h"
#include "Network/UStructuralPowerRCO.h"
#include "Routing/FStructuralPowerRouter.h"
#include "StructuralPowerConstants.h"
#include "StructuralPowerLog.h"
#include "UI/UStructuralPowerIdOptionManager.h"

namespace {
static FName ParseTypedId(const UEditableTextBox* TextBox) {
  if (!IsValid(TextBox)) {
    return NAME_None;
  }

  const FString Trimmed = TextBox->GetText().ToString().TrimStartAndEnd();
  return Trimmed.IsEmpty() ? NAME_None : FName(*Trimmed);
}

static bool HasTypedEntry(const UEditableTextBox* TextBox) {
  return IsValid(TextBox) && !TextBox->GetText().ToString().TrimStartAndEnd().IsEmpty();
}

static UStructuralPowerRCO* ResolveRco(AFGPlayerController* PlayerController) {
  return PlayerController ? PlayerController->GetRemoteCallObjectOfClass<UStructuralPowerRCO>()
                          : nullptr;
}
}

void FStructuralPowerIdApplyBridge::RequestComponentIdList(AFGPlayerController* PlayerController,
                                                           AFGBuildable* Target) {
  if (!IsValid(Target)) {
    return;
  }

  if (UStructuralPowerRCO* Rco = ResolveRco(PlayerController)) {
    Rco->Server_RequestComponentIdList(Target);
  }
}

void FStructuralPowerIdApplyBridge::ApplyEndpointIds(AFGPlayerController* PlayerController,
                                                     AFGBuildable* Target, const FName Source,
                                                     const FName Control, const bool bClearSource,
                                                     const bool bClearControl) {
  if (!IsValid(Target)) {
    return;
  }

  if (UStructuralPowerRCO* Rco = ResolveRco(PlayerController)) {
    Rco->Server_SetEndpointIds(Target, Source, Control, bClearSource, bClearControl);
  }
}

void FStructuralPowerIdApplyBridge::ApplySourceIndex(AFGPlayerController* PlayerController,
                                                     AFGBuildable* Target,
                                                     UStructuralPowerIdOptionManager* OptionManager,
                                                     const int32 Index) {
  if (!IsValid(Target) || !IsValid(OptionManager)) {
    return;
  }

  UStructuralPowerRCO* Rco = ResolveRco(PlayerController);
  if (!Rco) {
    return;
  }

  const FName Source = OptionManager->GetSourceIdFromIndex(Index);
  if (Source.IsNone()) {
    Rco->Server_SetEndpointIds(Target, NAME_None, NAME_None, true, false);
  } else if (FStructuralPowerRouter::IsPlayerChosenIdValid(Source)) {
    Rco->Server_SetEndpointIds(Target, Source, NAME_None, false, false);
  }

  RequestComponentIdList(PlayerController, Target);
}

void FStructuralPowerIdApplyBridge::ApplyControlIndex(
    AFGPlayerController* PlayerController, AFGBuildable* Target,
    UStructuralPowerIdOptionManager* OptionManager, const int32 Index) {
  if (!IsValid(Target) || !IsValid(OptionManager)) {
    return;
  }

  UStructuralPowerRCO* Rco = ResolveRco(PlayerController);
  if (!Rco) {
    return;
  }

  const FName Control = OptionManager->GetControlIdFromIndex(Index);
  if (Control.IsNone() || Control == StructuralPowerConstants::ControlUnconfigured) {
    Rco->Server_SetEndpointIds(Target, NAME_None, NAME_None, false, true);
  } else if (FStructuralPowerRouter::IsPlayerChosenIdValid(Control)) {
    Rco->Server_SetEndpointIds(Target, NAME_None, Control, false, false);
  }

  RequestComponentIdList(PlayerController, Target);
}

void FStructuralPowerIdApplyBridge::ApplyTypedIds(
    AFGPlayerController* PlayerController, AFGBuildable* Target,
    UStructuralPowerIdOptionManager* OptionManager, const UComboBoxString* SuggestedSourceCombo,
    const UComboBoxString* SuggestedControlCombo, const UEditableTextBox* AssignSourceText,
    const UEditableTextBox* AssignControlText, const UCheckBox* GlobalControlCheck) {
  if (!IsValid(Target) || !IsValid(OptionManager)) {
    UE_LOG(LogStructuralPower, Warning,
           TEXT("[HALSP] Id panel apply — missing target or option manager"));
    return;
  }

  UStructuralPowerRCO* Rco = ResolveRco(PlayerController);
  if (!Rco) {
    UE_LOG(LogStructuralPower, Warning, TEXT("[HALSP] Id panel apply — RCO missing"));
    return;
  }

  const bool bShowSource = OptionManager->ShowsSourceIdField();
  const bool bShowControl = OptionManager->ShowsControlIdField();
  bool bTouchSource = bShowSource && HasTypedEntry(AssignSourceText);
  bool bTouchControl = bShowControl && HasTypedEntry(AssignControlText);

  if (bShowSource && !bTouchSource && IsValid(SuggestedSourceCombo)) {
    const FName ComboSource =
        OptionManager->GetSourceIdFromIndex(SuggestedSourceCombo->GetSelectedIndex());
    if (FStructuralPowerRouter::IsPlayerChosenIdValid(ComboSource)) {
      bTouchSource = true;
    }
  }

  if (bShowControl && !bTouchControl && IsValid(SuggestedControlCombo)) {
    const FName ComboControl =
        OptionManager->GetControlIdFromIndex(SuggestedControlCombo->GetSelectedIndex());
    if (FStructuralPowerRouter::IsPlayerChosenIdValid(ComboControl)) {
      bTouchControl = true;
    }
  }

  const bool bTouchGlobal = IsValid(GlobalControlCheck);
  if (!bTouchSource && !bTouchControl && !bTouchGlobal) {
    UE_LOG(LogStructuralPower, Warning, TEXT("[HALSP] Id panel apply — no ids selected or typed"));
    return;
  }

  bool bClearSource = false;
  bool bClearControl = false;
  FName SourceToSet = NAME_None;
  FName ControlToSet = NAME_None;
  const bool bGlobalControl = bTouchGlobal && GlobalControlCheck->IsChecked();

  if (bTouchSource) {
    const FName TypedSource =
        HasTypedEntry(AssignSourceText)
            ? ParseTypedId(AssignSourceText)
            : (IsValid(SuggestedSourceCombo)
                   ? OptionManager->GetSourceIdFromIndex(SuggestedSourceCombo->GetSelectedIndex())
                   : NAME_None);
    if (TypedSource.IsNone()) {
      bClearSource = true;
    } else if (FStructuralPowerRouter::IsPlayerChosenIdValid(TypedSource)) {
      SourceToSet = TypedSource;
    } else {
      UE_LOG(LogStructuralPower, Warning, TEXT("[HALSP] Id panel apply — rejected source id '%s'"),
             *TypedSource.ToString());
      return;
    }
  }

  if (bTouchControl) {
    const FName TypedControl =
        HasTypedEntry(AssignControlText)
            ? ParseTypedId(AssignControlText)
            : (IsValid(SuggestedControlCombo)
                   ? OptionManager->GetControlIdFromIndex(SuggestedControlCombo->GetSelectedIndex())
                   : NAME_None);
    if (TypedControl.IsNone()) {
      bClearControl = true;
    } else if (FStructuralPowerRouter::IsPlayerChosenIdValid(TypedControl)) {
      ControlToSet = TypedControl;
    } else {
      UE_LOG(LogStructuralPower, Warning, TEXT("[HALSP] Id panel apply — rejected control id '%s'"),
             *TypedControl.ToString());
      return;
    }
  }

  Rco->Server_SetEndpointIds(Target, SourceToSet, ControlToSet, bClearSource, bClearControl,
                             bGlobalControl, bTouchGlobal);

  UE_LOG(LogStructuralPower, Log,
         TEXT("[HALSP] Id panel apply target=%s src=%s ctl=%s clearSrc=%d clearCtl=%d"
              " touchSrc=%d touchCtl=%d global=%d"),
         *Target->GetName(), bClearSource ? TEXT("(clear)") : *SourceToSet.ToString(),
         bClearControl ? TEXT("(clear)") : *ControlToSet.ToString(), bClearSource ? 1 : 0,
         bClearControl ? 1 : 0, bTouchSource ? 1 : 0, bTouchControl ? 1 : 0,
         bGlobalControl ? 1 : 0);

  RequestComponentIdList(PlayerController, Target);
}

void FStructuralPowerIdApplyBridge::ResetEndpointIds(AFGPlayerController* PlayerController,
                                                     AFGBuildable* Target, const bool bClearSource,
                                                     const bool bClearControl) {
  if (!IsValid(Target)) {
    return;
  }

  if (UStructuralPowerRCO* Rco = ResolveRco(PlayerController)) {
    Rco->Server_SetEndpointIds(Target, NAME_None, NAME_None, bClearSource, bClearControl);
  }

  RequestComponentIdList(PlayerController, Target);
}
