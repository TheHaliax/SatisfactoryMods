// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "UI/UStructuralPowerIdOptionManager.h"

#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Buildables/FGBuildableGenerator.h"
#include "Buildables/FGBuildableLightsControlPanel.h"
#include "FGOptionsLibrary.h"
#include "Routing/FStructuralMembershipRole.h"
#include "Routing/FStructuralPowerRouter.h"
#include "Rules/FStructuralEligibilityRules.h"
#include "Settings/FGUserSetting.h"
#include "Settings/FGUserSettingApplyType.h"
#include "Settings/FGUserSettingCategory.h"
#include "StructuralPowerConstants.h"
#include "UI/FGDynamicOptionsRow.h"

namespace {
static int32 FindOptionIndex(const TArray<FName>& Options, FName Id, int32 Fallback = 0) {
  if (Id.IsNone()) {
    return Fallback;
  }

  const int32 Index = Options.IndexOfByKey(Id);
  return Index == INDEX_NONE ? Fallback : Index;
}

static void AppendUniqueNamedIds(TArray<FName>& Out, const TArray<FName>& Ids) {
  for (const FName& NamedId : Ids) {
    if (FStructuralPowerRouter::IsPlayerChosenIdValid(NamedId)) {
      Out.AddUnique(NamedId);
    }
  }
}

static void FillIntSelector(UFGUserSetting_IntSelector* Selector, const TArray<FName>& Options,
                            int32 SelectedIndex) {
  if (!IsValid(Selector)) {
    return;
  }

  Selector->IntegerSelectionValues.Reset();
  Selector->ShowAsDropdown = true;
  Selector->BlockLastIndexFromManualSelection = false;

  for (int32 Index = 0; Index < Options.Num(); ++Index) {
    FIntegerSelection Entry;
    Entry.Value = Index;
    Entry.Name = FText::FromName(Options[Index]);
    Selector->IntegerSelectionValues.Add(Entry);
  }

  Selector->DefaultValue = FMath::Clamp(SelectedIndex, 0, FMath::Max(0, Options.Num() - 1));
}
}

void UStructuralPowerIdOptionManager::ResetSettings() {
  Settings.Reset();
  OwnedSettings.Reset();
  SourceOptions.Reset();
  ControlOptions.Reset();
}

EStructuralMembershipRole UStructuralPowerIdOptionManager::GetMembershipRole() const {
  return IsValid(TargetBuildable)
             ? FStructuralMembershipRole::Resolve(TargetBuildable, TargetChannel)
             : EStructuralMembershipRole::None;
}

bool UStructuralPowerIdOptionManager::ShowsSourceIdField() const {
  return FStructuralMembershipRole::UsesSourceField(GetMembershipRole());
}

bool UStructuralPowerIdOptionManager::ShowsControlIdField() const {
  return FStructuralMembershipRole::UsesControlField(GetMembershipRole());
}

void UStructuralPowerIdOptionManager::ConfigureForTarget(AFGBuildable* Target,
                                                         EStructuralChannel Channel) {
  TargetBuildable = Target;
  TargetChannel = Channel;
  ResetSettings();

  if (!IsValid(Target)) {
    return;
  }

  const EStructuralMembershipRole Role = GetMembershipRole();
  if (FStructuralMembershipRole::UsesSourceField(Role)) {
    SourceOptions = {NAME_None};
    CreateDropdownSetting(SourceSettingId, FText::FromString(TEXT("Suggested Source Ids")),
                          SourceOptions, 0);
  }

  if (FStructuralMembershipRole::UsesControlField(Role)) {
    ControlOptions = {NAME_None};
    if (Target->IsA<AFGBuildableLightsControlPanel>()) {
      ControlOptions[0] = StructuralPowerConstants::ControlUnconfigured;
    }

    CreateDropdownSetting(ControlSettingId, FText::FromString(TEXT("Suggested Control Ids")),
                          ControlOptions, 0);
  }
}

void UStructuralPowerIdOptionManager::SyncFromComponentList(
    const FStructuralComponentIdList& List) {
  if (ShowsSourceIdField() && !ShowsControlIdField()) {
    SourceOptions.Reset();
    AppendUniqueNamedIds(SourceOptions, List.NamedSourceIds);
    AppendUniqueNamedIds(SourceOptions, List.NamedSwitchControlIds);
    AppendUniqueNamedIds(SourceOptions, List.NamedLightGroupIds);

    if (SourceOptions.IsEmpty()) {
      SourceOptions.Add(NAME_None);
    }

    const FName ActiveSource =
        List.SourceOverride.IsNone() ? List.ResolvedSource : List.SourceOverride;
    if (FStructuralPowerRouter::IsPlayerChosenIdValid(ActiveSource)) {
      SourceOptions.AddUnique(ActiveSource);
    }

    const int32 Selected = FindOptionIndex(
        SourceOptions, List.SourceOverride.IsNone() ? List.ResolvedSource : List.SourceOverride, 0);

    if (UFGUserSettingApplyType* ApplyType = FindUserSetting(SourceSettingId)) {
      PopulateSelector(ApplyType->GetUserSetting(), SourceOptions, Selected);
      ApplyType->ForceSetValue(FVariant(Selected), true);
    }
  } else if (ShowsControlIdField() && !ShowsSourceIdField()) {
    ControlOptions.Reset();
    AppendUniqueNamedIds(ControlOptions, List.NamedControlIds);
    AppendUniqueNamedIds(ControlOptions, List.NamedSwitchControlIds);
    AppendUniqueNamedIds(ControlOptions, List.NamedLightGroupIds);

    if (ControlOptions.IsEmpty()) {
      ControlOptions.Add(NAME_None);
    }

    const FName ActiveControl =
        List.ControlOverride.IsNone() ? List.ResolvedControl : List.ControlOverride;
    if (FStructuralPowerRouter::IsPlayerChosenIdValid(ActiveControl)) {
      ControlOptions.AddUnique(ActiveControl);
    }

    const int32 Selected = FindOptionIndex(
        ControlOptions, List.ControlOverride.IsNone() ? List.ResolvedControl : List.ControlOverride,
        0);

    if (UFGUserSettingApplyType* ApplyType = FindUserSetting(ControlSettingId)) {
      PopulateSelector(ApplyType->GetUserSetting(), ControlOptions, Selected);
      ApplyType->ForceSetValue(FVariant(Selected), true);
    }
  } else if (ShowsSourceIdField() && ShowsControlIdField()) {
    SourceOptions.Reset();
    AppendUniqueNamedIds(SourceOptions, List.NamedSourceIds);
    AppendUniqueNamedIds(SourceOptions, List.NamedSwitchControlIds);

    if (SourceOptions.IsEmpty()) {
      SourceOptions.Add(NAME_None);
    }

    const FName ActiveSource =
        List.SourceOverride.IsNone() ? List.ResolvedSource : List.SourceOverride;
    if (FStructuralPowerRouter::IsPlayerChosenIdValid(ActiveSource)) {
      SourceOptions.AddUnique(ActiveSource);
    }

    const int32 SourceSelected = FindOptionIndex(SourceOptions, ActiveSource, 0);

    if (UFGUserSettingApplyType* ApplyType = FindUserSetting(SourceSettingId)) {
      PopulateSelector(ApplyType->GetUserSetting(), SourceOptions, SourceSelected);
      ApplyType->ForceSetValue(FVariant(SourceSelected), true);
    }

    ControlOptions.Reset();
    for (const FName& NamedId : List.NamedControlIds) {
      if (!NamedId.IsNone()) {
        ControlOptions.AddUnique(NamedId);
      }
    }

    const FName Fallback = TargetBuildable.IsA<AFGBuildableCircuitSwitch>()
                               ? NAME_None
                               : StructuralPowerConstants::ControlUnconfigured;

    if (ControlOptions.IsEmpty()) {
      ControlOptions.Add(Fallback);
    } else if (!ControlOptions.Contains(Fallback)) {
      ControlOptions.Insert(Fallback, 0);
    }

    const FName Active =
        List.ControlOverride.IsNone() ? List.ResolvedControl : List.ControlOverride;
    if (FStructuralPowerRouter::IsPlayerChosenIdValid(Active)) {
      ControlOptions.AddUnique(Active);
    }

    const int32 Selected = FindOptionIndex(ControlOptions, Active, 0);

    if (UFGUserSettingApplyType* ApplyType = FindUserSetting(ControlSettingId)) {
      PopulateSelector(ApplyType->GetUserSetting(), ControlOptions, Selected);
      ApplyType->ForceSetValue(FVariant(Selected), true);
    }
  }
}

FName UStructuralPowerIdOptionManager::GetSourceIdFromIndex(int32 Index) const {
  return SourceOptions.IsValidIndex(Index) ? SourceOptions[Index] : NAME_None;
}

FName UStructuralPowerIdOptionManager::GetControlIdFromIndex(int32 Index) const {
  return ControlOptions.IsValidIndex(Index) ? ControlOptions[Index] : NAME_None;
}

bool UStructuralPowerIdOptionManager::UsesSourceChannel() const {
  return ShowsSourceIdField() && !ShowsControlIdField();
}

bool UStructuralPowerIdOptionManager::UsesControlChannel() const {
  return ShowsControlIdField() && !ShowsSourceIdField();
}

bool UStructuralPowerIdOptionManager::NeedsDualFields() const {
  return ShowsSourceIdField() && ShowsControlIdField();
}

int32 UStructuralPowerIdOptionManager::GetSourceOptionCount() const {
  return SourceOptions.Num();
}

int32 UStructuralPowerIdOptionManager::GetControlOptionCount() const {
  return ControlOptions.Num();
}

FName UStructuralPowerIdOptionManager::GetSourceOptionAt(int32 Index) const {
  return GetSourceIdFromIndex(Index);
}

FName UStructuralPowerIdOptionManager::GetControlOptionAt(int32 Index) const {
  return GetControlIdFromIndex(Index);
}

int32 UStructuralPowerIdOptionManager::FindSourceOptionIndex(FName Id, int32 Fallback) const {
  return FindOptionIndex(SourceOptions, Id, Fallback);
}

int32 UStructuralPowerIdOptionManager::FindControlOptionIndex(FName Id, int32 Fallback) const {
  return FindOptionIndex(ControlOptions, Id, Fallback);
}

int32 UStructuralPowerIdOptionManager::GetSuggestedOptionCount() const {
  return UsesSourceChannel() ? SourceOptions.Num() : ControlOptions.Num();
}

FName UStructuralPowerIdOptionManager::GetSuggestedOptionAt(int32 Index) const {
  return UsesSourceChannel() ? GetSourceIdFromIndex(Index) : GetControlIdFromIndex(Index);
}

int32 UStructuralPowerIdOptionManager::FindSuggestedOptionIndex(FName Id, int32 Fallback) const {
  const TArray<FName>& Options = UsesSourceChannel() ? SourceOptions : ControlOptions;
  const int32 Index = Options.IndexOfByKey(Id);
  return Index == INDEX_NONE ? Fallback : Index;
}

UFGUserSetting* UStructuralPowerIdOptionManager::CreateDropdownSetting(const FString& SettingId,
                                                                       const FText& Label,
                                                                       const TArray<FName>& Options,
                                                                       int32 DefaultIndex) {
  UFGUserSetting* Setting = NewObject<UFGUserSetting>(this, *SettingId, RF_Transient);
  UFGUserSetting_IntSelector* Selector = NewObject<UFGUserSetting_IntSelector>(Setting);

  Setting->StrId = SettingId;
  Setting->DisplayName = Label;
  Setting->ValueSelector = Selector;
  Setting->UseCVar = false;
  Setting->VisibilityDisqualifiers = 0;
  Setting->ApplyType = UFGUserSettingApplyType_UpdateInstantly::StaticClass();

  FillIntSelector(Selector, Options, DefaultIndex);
  OwnedSettings.Add(Setting);
  RegisterSetting(Setting);
  return Setting;
}

void UStructuralPowerIdOptionManager::RegisterSetting(UFGUserSetting* Setting) {
  if (!IsValid(Setting)) {
    return;
  }

  if (UFGUserSettingApplyType* ApplyType =
          UFGUserSettingApplyType::GetUserSettingApplyType(this, Setting)) {
    ApplyType->Init(Setting);
    Settings.Add(Setting->StrId, ApplyType);
  }
}

void UStructuralPowerIdOptionManager::PopulateSelector(UFGUserSetting* Setting,
                                                       const TArray<FName>& Options,
                                                       int32 SelectedIndex) {
  if (!IsValid(Setting)) {
    return;
  }

  if (UFGUserSetting_IntSelector* Selector =
          Cast<UFGUserSetting_IntSelector>(Setting->ValueSelector)) {
    FillIntSelector(Selector, Options, SelectedIndex);
  }
}

void UStructuralPowerIdOptionManager::GetAllUserSettings(
    TArray<TObjectPtr<UFGUserSettingApplyType>>& OutUserSettings) const {
  Settings.GenerateValueArray(OutUserSettings);
}

void UStructuralPowerIdOptionManager::GetAllUserSettingsMap(
    TMap<FString, TObjectPtr<UFGUserSettingApplyType>>& OutUserSettings) const {
  OutUserSettings = Settings;
}

UFGUserSettingApplyType* UStructuralPowerIdOptionManager::FindUserSetting(
    const FString& SettingId) const {
  if (const TObjectPtr<UFGUserSettingApplyType>* Found = Settings.Find(SettingId)) {
    return Found->Get();
  }

  return nullptr;
}

IFGOptionInterface* UStructuralPowerIdOptionManager::GetPrimaryOptionInterface(
    UWorld* World) const {
  return const_cast<UStructuralPowerIdOptionManager*>(this);
}

TArray<FUserSettingCategoryMapping> UStructuralPowerIdOptionManager::GetCategorizedSettingWidgets(
    UObject* WorldContext, UUserWidget* OwningWidget) {
  TMap<FString, TObjectPtr<UFGUserSettingApplyType>> SettingsMap;
  GetAllUserSettingsMap(SettingsMap);

  TScriptInterface<IFGOptionInterface> OptionInterface(
      const_cast<UStructuralPowerIdOptionManager*>(this));
  return UFGOptionsLibrary::GetCategorizedUserSettingsWidgets(WorldContext, OwningWidget,
                                                              OptionInterface, SettingsMap);
}
