// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "UI/UStructuralPowerIdConfigWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/VerticalBox.h"
#include "FGPlayerController.h"
#include "Input/FStructuralPowerIdInput.h"
#include "Routing/FStructuralPowerRouter.h"
#include "Rules/FStructuralEligibilityRules.h"
#include "StructuralPowerLog.h"
#include "TimerManager.h"
#include "UI/FStructuralIdConfigTarget.h"
#include "UI/FStructuralPowerIdApplyBridge.h"
#include "UI/FStructuralPowerIdDisplaySync.h"
#include "UI/FStructuralPowerIdFieldMatrix.h"
#include "UI/FStructuralPowerIdModalHost.h"
#include "UI/FStructuralPowerIdShellBuilder.h"
#include "UI/FStructuralPowerIdWidgetHelpers.h"
#include "UI/FStructuralPowerIdWindowPool.h"
#include "UI/UStructuralPowerIdOptionManager.h"

using StructuralPowerIdUiHelpers::FormatIdOptionLabel;

FStructuralPowerIdShellWidgets UStructuralPowerIdConfigWidget::BuildShellWidgetRefs() const {
  return FStructuralPowerIdShellWidgets{
      .ShellBorder = ShellBorder,
      .TitleText = TitleText,
      .CloseButton = CloseButton,
      .ContentBorder = ContentBorder,
      .ContentVBox = ContentVBox,
  };
}

void UStructuralPowerIdConfigWidget::ApplyShellWidgetRefs(
    const FStructuralPowerIdShellWidgets& Shell) {
  ShellBorder = Shell.ShellBorder;
  TitleText = Shell.TitleText;
  CloseButton = Shell.CloseButton;
  ContentBorder = Shell.ContentBorder;
  ContentVBox = Shell.ContentVBox;
}

UStructuralPowerIdConfigWidget::UStructuralPowerIdConfigWidget(
    const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer) {
  bIsPopup = false;
  bIgnoreDefaultKeybindings = false;
  SetIsFocusable(true);
}

UStructuralPowerIdConfigWidget* UStructuralPowerIdConfigWidget::GetActiveWidget() {
  return FStructuralPowerIdWindowPool::GetActiveWidget();
}

UStructuralPowerIdConfigWidget* UStructuralPowerIdConfigWidget::GetPooledWindow() {
  return FStructuralPowerIdWindowPool::GetPooledWindow();
}

bool UStructuralPowerIdConfigWidget::IsPanelShown() const {
  return IsInViewport() && GetVisibility() != ESlateVisibility::Collapsed &&
         GetVisibility() != ESlateVisibility::Hidden;
}

bool UStructuralPowerIdConfigWidget::IsPanelVisible() const {
  return bModalActive && IsPanelShown();
}

bool UStructuralPowerIdConfigWidget::IsTextFieldFocused() const {
  return (IsValid(AssignSourceText) && AssignSourceText->HasKeyboardFocus()) ||
         (IsValid(AssignControlText) && AssignControlText->HasKeyboardFocus());
}

void UStructuralPowerIdConfigWidget::NormalizeModalState() {
  FStructuralPowerIdModalHost::NormalizeModalState(this);
}

UStructuralPowerIdConfigWidget*
UStructuralPowerIdConfigWidget::GetOrCreateWindow(AFGPlayerController* PC) {
  return FStructuralPowerIdWindowPool::GetOrCreateWindow(PC);
}

void UStructuralPowerIdConfigWidget::ResetStaticsForMapLoad() {
  FStructuralPowerIdWindowPool::ResetForMapLoad();
}

void UStructuralPowerIdConfigWidget::CloseActivePanel() {
  FStructuralPowerIdWindowPool::CloseActivePanel();
}

void UStructuralPowerIdConfigWidget::ReleaseForVanillaInteract(AFGPlayerController* PC) {
  FStructuralPowerIdWindowPool::ReleaseForVanillaInteract(PC);
}

void UStructuralPowerIdConfigWidget::NativeConstruct() {
  EnsureShellBuilt();
  Super::NativeConstruct();
}

void UStructuralPowerIdConfigWidget::NativeDestruct() {
  if (bModalActive) {
    if (AFGPlayerController* PC = GetOwningPlayer<AFGPlayerController>()) {
      FStructuralPowerIdModalHost::ForceReleaseAllModalState(PC);
    }
  }

  bInitializedForTarget = false;
  bModalActive = false;
  FStructuralPowerIdWindowPool::NotifyWindowDestroyed(this);

  Super::NativeDestruct();
}

void UStructuralPowerIdConfigWidget::ClearHotkeyTextBleed() {
  bSuppressHotkeyChar = true;

  if (IsValid(AssignSourceText)) {
    AssignSourceText->SetText(FText::GetEmpty());
  }

  if (IsValid(AssignControlText)) {
    AssignControlText->SetText(FText::GetEmpty());
  }

  if (UWorld* World = GetWorld()) {
    World->GetTimerManager().SetTimerForNextTick(
        FTimerDelegate::CreateWeakLambda(this, [this]() { bSuppressHotkeyChar = false; }));
  }
}

void UStructuralPowerIdConfigWidget::OpenForTarget(AFGBuildable* Target) {
  if (!IsValid(Target)) {
    return;
  }

  NormalizeModalState();
  EnsureShellBuilt();
  FStructuralPowerIdShellBuilder::EnsureViewportPresentation(this);
  SetIsEnabled(true);
  InitializeForTarget(Target);
  ClearHotkeyTextBleed();
  SetVisibility(ESlateVisibility::Visible);
  SetRenderOpacity(1.0f);

  if (IsInViewport()) {
    RemoveFromParent();
  }

  AddToViewport(ViewportZOrder);

  if (TSharedPtr<SWidget> SlateRoot = GetCachedWidget()) {
    SlateRoot->Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
  }

  if (AFGPlayerController* PC = GetOwningPlayer<AFGPlayerController>()) {
    FStructuralPowerIdModalHost::OnPanelOpened(this, PC);
  }

  UE_LOG(LogStructuralPower, Log,
         TEXT("[HALSP] Id panel opened target=%s inViewport=%d slate=%d cached=%d"),
         *Target->GetName(), IsInViewport() ? 1 : 0, GetCachedWidget().IsValid() ? 1 : 0,
         FStructuralPowerIdWindowPool::GetPooledWindow() == this ? 1 : 0);
}

FReply UStructuralPowerIdConfigWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry,
                                                              const FKeyEvent& InKeyEvent) {
  if (InKeyEvent.GetKey() == EKeys::Escape) {
    ClosePanel();
    return FReply::Handled();
  }

  if (bSuppressHotkeyChar && InKeyEvent.GetKey() == EKeys::I) {
    return FReply::Handled();
  }

  return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

FReply UStructuralPowerIdConfigWidget::NativeOnKeyChar(const FGeometry& InGeometry,
                                                       const FCharacterEvent& InCharacterEvent) {
  if (bSuppressHotkeyChar) {
    const TCHAR Char = InCharacterEvent.GetCharacter();
    if (Char == TEXT('i') || Char == TEXT('I')) {
      return FReply::Handled();
    }
  }

  return Super::NativeOnKeyChar(InGeometry, InCharacterEvent);
}

void UStructuralPowerIdConfigWidget::ForceReleaseAllModalState(AFGPlayerController* PC,
                                                               bool bRestoreGameInputMode) {
  FStructuralPowerIdModalHost::ForceReleaseAllModalState(PC, bRestoreGameInputMode);
}

void UStructuralPowerIdConfigWidget::InitializeForTarget(AFGBuildable* Target) {
  if (!IsValid(Target)) {
    return;
  }

  const bool bSameTarget = TargetBuildable == Target && bInitializedForTarget;
  TargetBuildable = Target;
  FStructuralPowerIdWindowPool::SetActiveWidget(this);
  EnsureShellBuilt();
  FStructuralPowerIdShellBuilder::UpdateWindowTitle(BuildShellWidgetRefs(), TargetBuildable);

  if (bSameTarget) {
    RequestIdList();
    return;
  }

  if (!IsValid(OptionManager)) {
    OptionManager = NewObject<UStructuralPowerIdOptionManager>(this);
  }

  const EStructuralChannel Channel = FStructuralIdConfigTarget::GetTargetChannel(Target);
  OptionManager->ConfigureForTarget(Target, Channel);
  RebuildPanelContent();
  bInitializedForTarget = true;
  FStructuralPowerIdDisplaySync::FlushPendingIdList(this);
  RequestIdList();

  const bool bLight = FStructuralEligibilityRules::IsStructuralLightConsumer(Target);
  const bool bSwitch = Target->IsA<AFGBuildableCircuitSwitch>();
  const TCHAR* Kind = bLight ? TEXT("Light") : (bSwitch ? TEXT("Switch") : TEXT("Panel"));
  UE_LOG(LogStructuralPower, Log,
         TEXT("[HALSP] Id panel generator init target=%s kind=%s options=%d root=%d"),
         *Target->GetName(), Kind, OptionManager->GetSuggestedOptionCount(),
         IsValid(WidgetTree) && IsValid(WidgetTree->RootWidget) ? 1 : 0);
}

void UStructuralPowerIdConfigWidget::RetargetTo(AFGBuildable* Target) {
  if (!IsValid(Target)) {
    return;
  }

  bInitializedForTarget = false;
  InitializeForTarget(Target);

  UE_LOG(LogStructuralPower, Log, TEXT("[HALSP] Id panel retargeted target=%s"),
         *Target->GetName());
}

void UStructuralPowerIdConfigWidget::ApplyComponentIdList(const FStructuralComponentIdList& List) {
  FStructuralPowerIdDisplaySync::ApplyComponentIdList(this, List);
}

void UStructuralPowerIdConfigWidget::EnsureShellBuilt() {
  FStructuralPowerIdShellWidgets Shell = BuildShellWidgetRefs();
  FStructuralPowerIdShellBuilder::EnsureShellBuilt(this, WidgetTree, Shell);
  ApplyShellWidgetRefs(Shell);

  if (IsValid(CloseButton)) {
    CloseButton->OnClicked.Clear();
    CloseButton->OnClicked.AddDynamic(this, &UStructuralPowerIdConfigWidget::OnCloseClicked);
  }
}

void UStructuralPowerIdConfigWidget::RebuildPanelContent() {
  EnsureShellBuilt();

  if (!IsValid(ContentVBox) || !IsValid(WidgetTree) || !IsValid(OptionManager) ||
      !IsValid(TargetBuildable)) {
    return;
  }

  ContentVBox->ClearChildren();
  SuggestedSourceCombo = nullptr;
  SuggestedControlCombo = nullptr;
  AssignSourceText = nullptr;
  AssignControlText = nullptr;
  GlobalControlCheck = nullptr;
  ApplyButton = nullptr;
  ResetButton = nullptr;
  ActiveIdsText = nullptr;

  FStructuralPowerIdFieldMatrixWidgets BuiltWidgets;
  if (!FStructuralPowerIdFieldMatrix::Build(WidgetTree, ContentVBox, TargetBuildable, OptionManager,
                                            BuiltWidgets)) {
    return;
  }

  SuggestedSourceCombo = BuiltWidgets.SuggestedSourceCombo;
  SuggestedControlCombo = BuiltWidgets.SuggestedControlCombo;
  AssignSourceText = BuiltWidgets.AssignSourceText;
  AssignControlText = BuiltWidgets.AssignControlText;
  GlobalControlCheck = BuiltWidgets.GlobalControlCheck;
  ApplyButton = BuiltWidgets.ApplyButton;
  ResetButton = BuiltWidgets.ResetButton;
  ActiveIdsText = BuiltWidgets.ActiveIdsText;

  if (IsValid(SuggestedSourceCombo)) {
    SuggestedSourceCombo->OnSelectionChanged.AddDynamic(
        this, &UStructuralPowerIdConfigWidget::OnSuggestedSourceComboChanged);
  }

  if (IsValid(SuggestedControlCombo)) {
    SuggestedControlCombo->OnSelectionChanged.AddDynamic(
        this, &UStructuralPowerIdConfigWidget::OnSuggestedControlComboChanged);
  }

  if (IsValid(ApplyButton)) {
    ApplyButton->OnClicked.AddDynamic(this, &UStructuralPowerIdConfigWidget::OnApplyTypedIdClicked);
  }

  if (IsValid(ResetButton)) {
    ResetButton->OnClicked.AddDynamic(this, &UStructuralPowerIdConfigWidget::OnResetIdClicked);
  }

  FStructuralPowerIdDisplaySync::FlushPendingIdList(this);
}

void UStructuralPowerIdConfigWidget::ApplySourceIndex(int32 Index) {
  if (bSuppressServerApply || !IsValid(TargetBuildable) || !IsValid(OptionManager)) {
    return;
  }

  AFGPlayerController* PC = GetOwningPlayer<AFGPlayerController>();
  FStructuralPowerIdApplyBridge::ApplySourceIndex(PC, TargetBuildable, OptionManager, Index);
}

void UStructuralPowerIdConfigWidget::ApplyControlIndex(int32 Index) {
  if (bSuppressServerApply || !IsValid(TargetBuildable) || !IsValid(OptionManager)) {
    return;
  }

  AFGPlayerController* PC = GetOwningPlayer<AFGPlayerController>();
  FStructuralPowerIdApplyBridge::ApplyControlIndex(PC, TargetBuildable, OptionManager, Index);
}

void UStructuralPowerIdConfigWidget::ApplySuggestedIndex(int32 Index) {
  if (!IsValid(OptionManager)) {
    return;
  }

  if (OptionManager->UsesSourceChannel()) {
    ApplySourceIndex(Index);
  } else if (OptionManager->UsesControlChannel()) {
    ApplyControlIndex(Index);
  }
}

void UStructuralPowerIdConfigWidget::OnSuggestedSourceComboChanged(
    FString SelectedItem, ESelectInfo::Type SelectionType) {
  if (bSuppressServerApply || SelectionType == ESelectInfo::Direct || !IsValid(OptionManager) ||
      !IsValid(AssignSourceText)) {
    return;
  }

  const int32 OptionCount = OptionManager->GetSourceOptionCount();
  for (int32 Index = 0; Index < OptionCount; ++Index) {
    if (FormatIdOptionLabel(OptionManager->GetSourceOptionAt(Index)) != SelectedItem) {
      continue;
    }

    const FName Id = OptionManager->GetSourceOptionAt(Index);
    if (FStructuralPowerRouter::IsPlayerChosenIdValid(Id)) {
      AssignSourceText->SetText(FText::FromName(Id));
    } else {
      AssignSourceText->SetText(FText::GetEmpty());
    }

    return;
  }
}

void UStructuralPowerIdConfigWidget::OnSuggestedControlComboChanged(
    FString SelectedItem, ESelectInfo::Type SelectionType) {
  if (bSuppressServerApply || SelectionType == ESelectInfo::Direct || !IsValid(OptionManager) ||
      !IsValid(AssignControlText)) {
    return;
  }

  const int32 OptionCount = OptionManager->GetControlOptionCount();
  for (int32 Index = 0; Index < OptionCount; ++Index) {
    if (FormatIdOptionLabel(OptionManager->GetControlOptionAt(Index)) != SelectedItem) {
      continue;
    }

    const FName Id = OptionManager->GetControlOptionAt(Index);
    if (FStructuralPowerRouter::IsPlayerChosenIdValid(Id)) {
      AssignControlText->SetText(FText::FromName(Id));
    } else {
      AssignControlText->SetText(FText::GetEmpty());
    }

    return;
  }
}

void UStructuralPowerIdConfigWidget::ApplyTypedIdsToServer() {
  if (!IsValid(TargetBuildable) || !IsValid(OptionManager)) {
    return;
  }

  AFGPlayerController* PC = GetOwningPlayer<AFGPlayerController>();
  FStructuralPowerIdApplyBridge::ApplyTypedIds(
      PC, TargetBuildable, OptionManager, SuggestedSourceCombo, SuggestedControlCombo,
      AssignSourceText, AssignControlText, GlobalControlCheck);
}

void UStructuralPowerIdConfigWidget::OnApplyTypedIdClicked() {
  ApplyTypedIdsToServer();
}

void UStructuralPowerIdConfigWidget::OnResetIdClicked() {
  if (!IsValid(TargetBuildable)) {
    return;
  }

  if (AssignSourceText) {
    AssignSourceText->SetText(FText::GetEmpty());
  }

  if (AssignControlText) {
    AssignControlText->SetText(FText::GetEmpty());
  }

  AFGPlayerController* PC = GetOwningPlayer<AFGPlayerController>();
  const bool bClearSource = IsValid(OptionManager) && OptionManager->ShowsSourceIdField();
  const bool bClearControl = IsValid(OptionManager) && OptionManager->ShowsControlIdField();
  FStructuralPowerIdApplyBridge::ResetEndpointIds(PC, TargetBuildable, bClearSource, bClearControl);
}

void UStructuralPowerIdConfigWidget::RequestIdList() {
  if (!IsValid(TargetBuildable)) {
    return;
  }

  if (AFGPlayerController* PC = GetOwningPlayer<AFGPlayerController>()) {
    FStructuralPowerIdApplyBridge::RequestComponentIdList(PC, TargetBuildable);
  }
}

void UStructuralPowerIdConfigWidget::OnCloseClicked() {
  ClosePanel();
}

void UStructuralPowerIdConfigWidget::ClosePanel() {
  if (!bModalActive) {
    return;
  }

  if (FStructuralPowerIdWindowPool::GetActiveWidget() == this) {
    FStructuralPowerIdWindowPool::SetActiveWidget(nullptr);
  }

  bInitializedForTarget = false;
  TargetBuildable = nullptr;
  bModalActive = false;
  FStructuralPowerIdModalHost::DetachFromViewport(this);

  if (AFGPlayerController* PC = GetOwningPlayer<AFGPlayerController>()) {
    FStructuralPowerIdModalHost::OnPanelClosed(this, PC);
  }

  UE_LOG(LogStructuralPower, Log, TEXT("[HALSP] Id panel closed inViewport=%d cached=%d"),
         IsInViewport() ? 1 : 0, FStructuralPowerIdWindowPool::GetPooledWindow() == this ? 1 : 0);
}
