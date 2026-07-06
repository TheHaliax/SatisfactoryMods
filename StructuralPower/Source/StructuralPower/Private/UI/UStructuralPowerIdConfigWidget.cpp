// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "UI/UStructuralPowerIdConfigWidget.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/NamedSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "FGHUD.h"
#include "FGPlayerController.h"
#include "FGPlayerControllerBase.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "Input/Events.h"
#include "Input/FStructuralPowerIdInput.h"
#include "UI/FStructuralPowerIdApplyBridge.h"
#include "UI/FStructuralPowerIdFieldMatrix.h"
#include "UI/FStructuralPowerIdWidgetHelpers.h"
#include "Config/FStructuralPowerModConfig.h"
#include "Routing/FStructuralPowerRouter.h"
#include "Rules/FStructuralEligibilityRules.h"
#include "Settings/FGUserSetting.h"
#include "Settings/FGUserSettingApplyType.h"
#include "StructuralPowerConstants.h"
#include "StructuralPowerLog.h"
#include "TimerManager.h"
#include "UI/FStructuralIdConfigTarget.h"
#include "UI/FGGameUI.h"
#include "UI/Message/FGAudioMessage.h"
#include "UI/UStructuralPowerIdOptionManager.h"

using StructuralPowerIdUiHelpers::AddVBoxRow;
using StructuralPowerIdUiHelpers::FormatIdOptionLabel;
using StructuralPowerIdUiHelpers::MakeLabeledButton;
using StructuralPowerIdUiHelpers::SetTextStyle;

namespace
{
class FStructuralPowerIdEscapeProcessor : public IInputProcessor
{
public:
	virtual bool HandleKeyDownEvent(
		FSlateApplication& SlateApp,
		const FKeyEvent& InKeyEvent) override
	{
		if (InKeyEvent.GetKey() != EKeys::Escape)
		{
			return false;
		}

		UStructuralPowerIdConfigWidget* Active =
			UStructuralPowerIdConfigWidget::GetActiveWidget();
		if (!IsValid(Active) || !Active->IsPanelVisible())
		{
			return false;
		}

		Active->ClosePanel();
		return true;
	}

	virtual void Tick(
		const float DeltaTime,
		FSlateApplication& SlateApp,
		TSharedRef<ICursor> Cursor) override
	{
	}
};

static void WalkWidgetTree(UWidget* Widget, TFunctionRef<void(UWidget*)> Visitor)
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

static bool MountWidgetInHost(UWidget* Host, UWidget* Child)
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

static UPanelWidget* FindWindowBodyPanel(UUserWidget* Window)
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

static void DismissBlockingGameMessages(AFGPlayerController* PC)
{
	if (!IsValid(PC))
	{
		return;
	}

	UFGGameUI* GameUI = PC->GetGameUI();
	if (!IsValid(GameUI))
	{
		return;
	}

	if (UFGAudioMessage* ActiveMessage = GameUI->GetActiveAudioMessage())
	{
		ActiveMessage->CancelPlayback();
		GameUI->AudioMessageFinishedPlayback();
		UE_LOG(LogStructuralPower, Log, TEXT("[PWR] Id panel dismissed active audio message"));
	}
}
}

TWeakObjectPtr<UStructuralPowerIdConfigWidget> UStructuralPowerIdConfigWidget::ActiveInstance;
TObjectPtr<UStructuralPowerIdConfigWidget> UStructuralPowerIdConfigWidget::CachedWindow;
TSharedPtr<IInputProcessor> UStructuralPowerIdConfigWidget::EscapeInputProcessor;

UStructuralPowerIdConfigWidget::UStructuralPowerIdConfigWidget(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsPopup = false;
	bIgnoreDefaultKeybindings = false;
	SetIsFocusable(true);
}

UStructuralPowerIdConfigWidget* UStructuralPowerIdConfigWidget::GetActiveWidget()
{
	return ActiveInstance.Get();
}

UStructuralPowerIdConfigWidget* UStructuralPowerIdConfigWidget::GetPooledWindow()
{
	return CachedWindow.Get();
}

bool UStructuralPowerIdConfigWidget::IsPanelShown() const
{
	return IsInViewport()
		&& GetVisibility() != ESlateVisibility::Collapsed
		&& GetVisibility() != ESlateVisibility::Hidden;
}

bool UStructuralPowerIdConfigWidget::IsPanelVisible() const
{
	return bModalActive && IsPanelShown();
}

bool UStructuralPowerIdConfigWidget::IsTextFieldFocused() const
{
	return (IsValid(AssignSourceText) && AssignSourceText->HasKeyboardFocus())
		|| (IsValid(AssignControlText) && AssignControlText->HasKeyboardFocus());
}

void UStructuralPowerIdConfigWidget::NormalizeModalState()
{
	if (bModalActive && !IsPanelShown())
	{
		bModalActive = false;
		if (ActiveInstance.Get() == this)
		{
			SetActiveWidget(nullptr);
		}

		if (AFGPlayerController* PC = GetOwningPlayer<AFGPlayerController>())
		{
			UnregisterEscapeInputProcessor();
			ForceReleaseAllModalState(PC);
		}

		UE_LOG(LogStructuralPower, Warning,
			TEXT("[PWR] Id panel recovered stale modal (active=1 shown=0)"));
	}
	else if (!bModalActive && IsPanelShown())
	{
		SetVisibility(ESlateVisibility::Hidden);
		SetIsEnabled(false);
	}
}

UStructuralPowerIdConfigWidget* UStructuralPowerIdConfigWidget::GetOrCreateWindow(
	AFGPlayerController* PC)
{
	if (!IsValid(PC))
	{
		return nullptr;
	}

	if (UStructuralPowerIdConfigWidget* Existing = CachedWindow.Get())
	{
		if (IsValid(Existing))
		{
			return Existing;
		}

		CachedWindow = nullptr;
	}

	UStructuralPowerIdConfigWidget* Window = CreateWidget<UStructuralPowerIdConfigWidget>(
		PC,
		StaticClass());
	if (!IsValid(Window))
	{
		return nullptr;
	}

	CachedWindow = Window;
	UE_LOG(LogStructuralPower, Log, TEXT("[PWR] Id panel window created"));
	return Window;
}

void UStructuralPowerIdConfigWidget::SetActiveWidget(UStructuralPowerIdConfigWidget* Widget)
{
	if (Widget == nullptr)
	{
		ActiveInstance.Reset();
		return;
	}

	if (!IsValid(Widget))
	{
		ActiveInstance.Reset();
		return;
	}

	ActiveInstance = Widget;
}

void UStructuralPowerIdConfigWidget::ResetStaticsForMapLoad()
{
	ActiveInstance.Reset();
	CachedWindow = nullptr;
	UnregisterEscapeInputProcessor();
}

void UStructuralPowerIdConfigWidget::CloseActivePanel()
{
	if (UStructuralPowerIdConfigWidget* Widget = ActiveInstance.Get())
	{
		Widget->ClosePanel();
		return;
	}

	if (UStructuralPowerIdConfigWidget* Window = CachedWindow.Get())
	{
		Window->DetachFromViewport();
		if (AFGPlayerController* PC = Window->GetOwningPlayer<AFGPlayerController>())
		{
			ForceReleaseAllModalState(PC);
		}
	}
}

void UStructuralPowerIdConfigWidget::ReleaseForVanillaInteract(AFGPlayerController* PC)
{
	if (!IsValid(PC))
	{
		return;
	}

	if (UStructuralPowerIdConfigWidget* Widget = ActiveInstance.Get())
	{
		Widget->DetachFromViewport();
		Widget->bModalActive = false;
		if (ActiveInstance.Get() == Widget)
		{
			SetActiveWidget(nullptr);
		}
	}
	else if (UStructuralPowerIdConfigWidget* Window = CachedWindow.Get())
	{
		if (IsValid(Window))
		{
			Window->DetachFromViewport();
			Window->bModalActive = false;
		}
	}

	ForceReleaseAllModalState(PC, /*bRestoreGameInputMode=*/false);
}

void UStructuralPowerIdConfigWidget::DetachFromViewport()
{
	UnregisterEscapeInputProcessor();
	SetVisibility(ESlateVisibility::Collapsed);
	SetIsEnabled(false);

	if (IsInViewport())
	{
		RemoveFromParent();
	}
}

void UStructuralPowerIdConfigWidget::NativeConstruct()
{
	EnsureShellBuilt();
	Super::NativeConstruct();
}

void UStructuralPowerIdConfigWidget::NativeDestruct()
{
	if (bModalActive)
	{
		if (AFGPlayerController* PC = GetOwningPlayer<AFGPlayerController>())
		{
			ForceReleaseAllModalState(PC);
		}
	}

	if (ActiveInstance.Get() == this)
	{
		SetActiveWidget(nullptr);
	}

	bInitializedForTarget = false;
	bModalActive = false;

	if (CachedWindow.Get() == this)
	{
		CachedWindow = nullptr;
	}

	Super::NativeDestruct();
}

void UStructuralPowerIdConfigWidget::EnsureViewportPresentation()
{
	SetAnchorsInViewport(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	SetAlignmentInViewport(FVector2D::ZeroVector);
}

void UStructuralPowerIdConfigWidget::ClearHotkeyTextBleed()
{
	bSuppressHotkeyChar = true;

	if (IsValid(AssignSourceText))
	{
		AssignSourceText->SetText(FText::GetEmpty());
	}

	if (IsValid(AssignControlText))
	{
		AssignControlText->SetText(FText::GetEmpty());
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				bSuppressHotkeyChar = false;
			}));
	}
}

void UStructuralPowerIdConfigWidget::OpenForTarget(AFGBuildable* Target)
{
	if (!IsValid(Target))
	{
		return;
	}

	NormalizeModalState();
	EnsureShellBuilt();
	EnsureViewportPresentation();
	SetIsEnabled(true);
	InitializeForTarget(Target);
	ClearHotkeyTextBleed();
	SetVisibility(ESlateVisibility::Visible);
	SetRenderOpacity(1.0f);

	if (IsInViewport())
	{
		RemoveFromParent();
	}

	AddToViewport(ViewportZOrder);

	if (TSharedPtr<SWidget> SlateRoot = GetCachedWidget())
	{
		SlateRoot->Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
	}

	SetActiveWidget(this);
	bModalActive = true;
	RegisterEscapeInputProcessor();
	ApplyModalInputMode();
	ScheduleModalInputModeRefresh();

	if (AFGPlayerController* PC = GetOwningPlayer<AFGPlayerController>())
	{
		DismissBlockingGameMessages(PC);
		FStructuralPowerIdInput::NotifyPanelOpened(PC);
	}

	UE_LOG(LogStructuralPower, Log,
		TEXT("[PWR] Id panel opened target=%s inViewport=%d slate=%d cached=%d"),
		*Target->GetName(),
		IsInViewport() ? 1 : 0,
		GetCachedWidget().IsValid() ? 1 : 0,
		CachedWindow.Get() == this ? 1 : 0);
}

FReply UStructuralPowerIdConfigWidget::NativeOnPreviewKeyDown(
	const FGeometry& InGeometry,
	const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		ClosePanel();
		return FReply::Handled();
	}

	if (bSuppressHotkeyChar && InKeyEvent.GetKey() == EKeys::I)
	{
		return FReply::Handled();
	}

	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

FReply UStructuralPowerIdConfigWidget::NativeOnKeyChar(
	const FGeometry& InGeometry,
	const FCharacterEvent& InCharacterEvent)
{
	if (bSuppressHotkeyChar)
	{
		const TCHAR Char = InCharacterEvent.GetCharacter();
		if (Char == TEXT('i') || Char == TEXT('I'))
		{
			return FReply::Handled();
		}
	}

	return Super::NativeOnKeyChar(InGeometry, InCharacterEvent);
}

void UStructuralPowerIdConfigWidget::ApplyModalInputMode()
{
	if (!bModalActive)
	{
		return;
	}

	AFGPlayerController* PC = GetOwningPlayer<AFGPlayerController>();
	if (!IsValid(PC))
	{
		return;
	}

	if (APawn* Pawn = PC->GetPawn())
	{
		Pawn->DisableInput(PC);
	}

	PC->SetIgnoreLookInput(true);
	PC->SetIgnoreMoveInput(true);
	PC->bShowMouseCursor = true;
	PC->bEnableClickEvents = true;
	PC->bEnableMouseOverEvents = true;

	FInputModeUIOnly InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);

	UWidget* FocusWidget = IsValid(CloseButton)
		? static_cast<UWidget*>(CloseButton.Get())
		: static_cast<UWidget*>(this);
	TSharedPtr<SWidget> SlateWidget = FocusWidget->GetCachedWidget();
	if (!SlateWidget.IsValid())
	{
		SlateWidget = GetCachedWidget();
	}

	if (SlateWidget.IsValid())
	{
		InputMode.SetWidgetToFocus(SlateWidget);
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().SetKeyboardFocus(SlateWidget, EFocusCause::SetDirectly);
			const int32 UserIndex = PC->GetLocalPlayer()
				? PC->GetLocalPlayer()->GetControllerId()
				: 0;
			FSlateApplication::Get().SetUserFocus(UserIndex, SlateWidget, EFocusCause::SetDirectly);
		}
	}

	PC->SetInputMode(InputMode);

	if (AFGHUD* HUD = PC->GetHUD<AFGHUD>())
	{
		HUD->SetForceHideCrossHair(true);
		HUD->SetShowCrossHair(false);
	}

	UE_LOG(LogStructuralPower, Verbose,
		TEXT("[PWR] Id panel input=UIOnly cursor=%d slateFocus=%d inViewport=%d"),
		PC->bShowMouseCursor ? 1 : 0,
		SlateWidget.IsValid() ? 1 : 0,
		IsInViewport() ? 1 : 0);
}

void UStructuralPowerIdConfigWidget::ScheduleModalInputModeRefresh()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(this, &UStructuralPowerIdConfigWidget::ApplyModalInputMode));
	}
}

void UStructuralPowerIdConfigWidget::ForceReleaseAllModalState(
	AFGPlayerController* PC,
	bool bRestoreGameInputMode)
{
	if (!IsValid(PC))
	{
		return;
	}

	UnregisterEscapeInputProcessor();

	PC->ResetIgnoreMoveInput();
	PC->ResetIgnoreLookInput();

	if (APawn* Pawn = PC->GetPawn())
	{
		Pawn->EnableInput(PC);
	}

	if (!bRestoreGameInputMode)
	{
		UE_LOG(LogStructuralPower, Verbose,
			TEXT("[PWR] Id panel released capture (vanilla interact owns input)"));
		return;
	}

	if (UFGGameUI* GameUI = PC->GetGameUI())
	{
		if (IsValid(GameUI) && GameUI->HasActiveInteractWidget())
		{
			UE_LOG(LogStructuralPower, Verbose,
				TEXT("[PWR] Id panel skip GameOnly — vanilla interact active"));
			return;
		}
	}

	PC->bShowMouseCursor = false;
	PC->bEnableClickEvents = false;
	PC->bEnableMouseOverEvents = false;

	FInputModeGameOnly InputMode;
	PC->SetInputMode(InputMode);
	PC->FlushPressedKeys();

	if (AFGHUD* HUD = PC->GetHUD<AFGHUD>())
	{
		HUD->SetForceHideCrossHair(false);
		HUD->SetShowCrossHair(true);
	}

	UE_LOG(LogStructuralPower, Log, TEXT("[PWR] Id panel force-released modal state"));
}

void UStructuralPowerIdConfigWidget::RegisterEscapeInputProcessor()
{
	if (EscapeInputProcessor.IsValid())
	{
		return;
	}

	EscapeInputProcessor = MakeShared<FStructuralPowerIdEscapeProcessor>();
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().RegisterInputPreProcessor(EscapeInputProcessor, 0);
	}
}

void UStructuralPowerIdConfigWidget::UnregisterEscapeInputProcessor()
{
	if (!EscapeInputProcessor.IsValid())
	{
		return;
	}

	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(EscapeInputProcessor);
	}

	EscapeInputProcessor.Reset();
}

void UStructuralPowerIdConfigWidget::InitializeForTarget(AFGBuildable* Target)
{
	if (!IsValid(Target))
	{
		return;
	}

	const bool bSameTarget = TargetBuildable == Target && bInitializedForTarget;
	TargetBuildable = Target;
	SetActiveWidget(this);
	EnsureShellBuilt();
	UpdateWindowTitle();

	if (bSameTarget)
	{
		RequestIdList();
		return;
	}

	if (!IsValid(OptionManager))
	{
		OptionManager = NewObject<UStructuralPowerIdOptionManager>(this);
	}

	const EStructuralChannel Channel = FStructuralIdConfigTarget::GetTargetChannel(Target);
	OptionManager->ConfigureForTarget(Target, Channel);
	RebuildPanelContent();
	bInitializedForTarget = true;
	FlushPendingIdList();
	RequestIdList();

	const bool bLight = FStructuralEligibilityRules::IsStructuralLightConsumer(Target);
	const bool bSwitch = Target->IsA<AFGBuildableCircuitSwitch>();
	const TCHAR* Kind = bLight ? TEXT("Light") : (bSwitch ? TEXT("Switch") : TEXT("Panel"));
	UE_LOG(LogStructuralPower, Log,
		TEXT("[PWR] Id panel generator init target=%s kind=%s options=%d root=%d"),
		*Target->GetName(),
		Kind,
		OptionManager->GetSuggestedOptionCount(),
		IsValid(WidgetTree) && IsValid(WidgetTree->RootWidget) ? 1 : 0);
}

void UStructuralPowerIdConfigWidget::RetargetTo(AFGBuildable* Target)
{
	if (!IsValid(Target))
	{
		return;
	}

	bInitializedForTarget = false;
	InitializeForTarget(Target);

	UE_LOG(LogStructuralPower, Log,
		TEXT("[PWR] Id panel retargeted target=%s"),
		*Target->GetName());
}

void UStructuralPowerIdConfigWidget::ApplyComponentIdList(const FStructuralComponentIdList& List)
{
	PendingIdList = List;
	bHasPendingIdList = true;

	if (!IsValid(OptionManager))
	{
		UE_LOG(LogStructuralPower, Verbose,
			TEXT("[PWR] Id panel sync deferred — option manager not ready"));
		return;
	}

	OptionManager->SyncFromComponentList(List);

	if (!AreFormWidgetsReady())
	{
		UE_LOG(LogStructuralPower, Verbose,
			TEXT("[PWR] Id panel sync deferred — form widgets not ready"));
		return;
	}

	RefreshIdDisplayFromList(List);
	bHasPendingIdList = false;
}

bool UStructuralPowerIdConfigWidget::AreFormWidgetsReady() const
{
	if (!IsValid(OptionManager) || !IsValid(TargetBuildable))
	{
		return false;
	}

	if (!IsValid(SuggestedSourceCombo) || !IsValid(AssignSourceText))
	{
		return false;
	}

	if (OptionManager->NeedsDualFields())
	{
		return IsValid(SuggestedControlCombo) && IsValid(AssignControlText);
	}

	return true;
}

void UStructuralPowerIdConfigWidget::FlushPendingIdList()
{
	if (!bHasPendingIdList || !IsValid(OptionManager) || !AreFormWidgetsReady())
	{
		return;
	}

	OptionManager->SyncFromComponentList(PendingIdList);
	RefreshIdDisplayFromList(PendingIdList);
	bHasPendingIdList = false;
}

void UStructuralPowerIdConfigWidget::RepopulateComboFromManager(
	UComboBoxString* Combo,
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

void UStructuralPowerIdConfigWidget::RefreshIdDisplayFromList(
	const FStructuralComponentIdList& List)
{
	if (!IsValid(OptionManager))
	{
		return;
	}

	bSuppressServerApply = true;

	RepopulateComboFromManager(SuggestedSourceCombo, true);
	if (OptionManager->NeedsDualFields())
	{
		RepopulateComboFromManager(SuggestedControlCombo, false);
	}

	const FName ActiveSource = List.SourceOverride.IsNone()
		? List.ResolvedSource
		: List.SourceOverride;
	const FName ActiveControl = List.ControlOverride.IsNone()
		? List.ResolvedControl
		: List.ControlOverride;

	if (IsValid(SuggestedSourceCombo))
	{
		const int32 SourceIndex = OptionManager->FindSourceOptionIndex(ActiveSource, 0);
		SuggestedSourceCombo->SetSelectedIndex(SourceIndex);
	}

	if (IsValid(SuggestedControlCombo))
	{
		const int32 ControlIndex = OptionManager->FindControlOptionIndex(ActiveControl, 0);
		SuggestedControlCombo->SetSelectedIndex(ControlIndex);
	}

	if (IsValid(AssignSourceText))
	{
		if (!List.SourceOverride.IsNone()
			&& FStructuralPowerRouter::IsPlayerChosenIdValid(List.SourceOverride))
		{
			AssignSourceText->SetText(FText::FromName(List.SourceOverride));
			AssignSourceText->SetHintText(FText::FromString(TEXT("Enter id name")));
		}
		else
		{
			AssignSourceText->SetText(FText::GetEmpty());
			const FString Hint = List.ResolvedSource.IsNone()
				? TEXT("Enter id name")
				: FString::Printf(TEXT("Auto: %s"), *List.ResolvedSource.ToString());
			AssignSourceText->SetHintText(FText::FromString(Hint));
		}
	}

	if (IsValid(AssignControlText))
	{
		if (!List.ControlOverride.IsNone()
			&& FStructuralPowerRouter::IsPlayerChosenIdValid(List.ControlOverride))
		{
			AssignControlText->SetText(FText::FromName(List.ControlOverride));
			AssignControlText->SetHintText(FText::FromString(TEXT("Enter id name")));
		}
		else
		{
			AssignControlText->SetText(FText::GetEmpty());
			const FString Hint = List.ResolvedControl.IsNone()
				|| List.ResolvedControl == StructuralPowerConstants::ControlUnconfigured
				|| List.ResolvedControl == StructuralPowerConstants::ControlBypass
				? TEXT("Enter id name")
				: FString::Printf(TEXT("Auto: %s"), *List.ResolvedControl.ToString());
			AssignControlText->SetHintText(FText::FromString(Hint));
		}
	}

	if (IsValid(ActiveIdsText))
	{
		const FString SourceLine = List.SourceOverride.IsNone()
			? (List.ResolvedSource.IsNone() ? TEXT("-") : List.ResolvedSource.ToString())
			: FString::Printf(TEXT("%s*"), *List.SourceOverride.ToString());
		const FString ControlLine = List.ControlOverride.IsNone()
			? (List.ResolvedControl.IsNone() ? TEXT("-") : List.ResolvedControl.ToString())
			: FString::Printf(TEXT("%s*"), *List.ControlOverride.ToString());

		const bool bDual = OptionManager->NeedsDualFields();
		const FString Status = bDual
			? FString::Printf(TEXT("Active — source: %s   control: %s"), *SourceLine, *ControlLine)
			: FString::Printf(TEXT("Active — source: %s"), *SourceLine);
		SetTextStyle(ActiveIdsText, Status, 12);
	}

	bSuppressServerApply = false;

	UE_LOG(LogStructuralPower, Log,
		TEXT("[PWR] Id panel synced ids resolvedSrc=%s resolvedCtl=%s overrideSrc=%s overrideCtl=%s"),
		List.ResolvedSource.IsNone() ? TEXT("-") : *List.ResolvedSource.ToString(),
		List.ResolvedControl.IsNone() ? TEXT("-") : *List.ResolvedControl.ToString(),
		List.SourceOverride.IsNone() ? TEXT("-") : *List.SourceOverride.ToString(),
		List.ControlOverride.IsNone() ? TEXT("-") : *List.ControlOverride.ToString());
}

void UStructuralPowerIdConfigWidget::UpdateWindowTitle()
{
	if (!IsValid(TitleText) || !IsValid(TargetBuildable))
	{
		return;
	}

	const bool bLight = FStructuralEligibilityRules::IsStructuralLightConsumer(TargetBuildable);
	const bool bSwitch = TargetBuildable->IsA<AFGBuildableCircuitSwitch>();
	const TCHAR* Kind = bLight ? TEXT("Light") : (bSwitch ? TEXT("Switch") : TEXT("Panel"));
	SetTextStyle(
		TitleText,
		FString::Printf(TEXT("Structural Power — %s"), Kind),
		15,
		true);
}

void UStructuralPowerIdConfigWidget::ResetShellState()
{
	ShellBorder = nullptr;
	TitleText = nullptr;
	CloseButton = nullptr;
	ContentBorder = nullptr;
	ContentVBox = nullptr;
	SuggestedSourceCombo = nullptr;
	SuggestedControlCombo = nullptr;
	AssignSourceText = nullptr;
	AssignControlText = nullptr;
	ApplyButton = nullptr;
	ResetButton = nullptr;
	ActiveIdsText = nullptr;

	if (WidgetTree)
	{
		WidgetTree->RootWidget = nullptr;
	}
}

void UStructuralPowerIdConfigWidget::EnsureShellBuilt()
{
	if (IsValid(ContentVBox) && IsValid(WidgetTree) && IsValid(WidgetTree->RootWidget))
	{
		return;
	}

	ResetShellState();

	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	}

	UOverlay* RootOverlay = WidgetTree->ConstructWidget<UOverlay>(
		UOverlay::StaticClass(),
		TEXT("RootOverlay"));
	RootOverlay->SetVisibility(ESlateVisibility::Visible);
	RootOverlay->SetClipping(EWidgetClipping::ClipToBoundsAlways);

	UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("ModalBackdrop"));
	Backdrop->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.45f));
	Backdrop->SetPadding(FMargin(0.0f));
	RootOverlay->AddChild(Backdrop);
	if (UOverlaySlot* BackdropSlot = Cast<UOverlaySlot>(Backdrop->Slot))
	{
		BackdropSlot->SetHorizontalAlignment(HAlign_Fill);
		BackdropSlot->SetVerticalAlignment(VAlign_Fill);
	}

	USizeBox* Frame = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RootFrame"));
	Frame->SetWidthOverride(500.0f);
	Frame->SetMinDesiredHeight(380.0f);
	Frame->SetVisibility(ESlateVisibility::Visible);
	RootOverlay->AddChild(Frame);
	if (UOverlaySlot* FrameSlot = Cast<UOverlaySlot>(Frame->Slot))
	{
		FrameSlot->SetHorizontalAlignment(HAlign_Center);
		FrameSlot->SetVerticalAlignment(VAlign_Center);
	}

	ShellBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ShellBorder"));
	ShellBorder->SetBrushColor(FLinearColor(0.03f, 0.05f, 0.07f, 0.98f));
	ShellBorder->SetPadding(FMargin(0.0f));
	Frame->AddChild(ShellBorder);

	UVerticalBox* ShellVBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(),
		TEXT("ShellVBox"));
	ShellBorder->AddChild(ShellVBox);

	UBorder* TitleBar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TitleBar"));
	TitleBar->SetBrushColor(FLinearColor(0.06f, 0.08f, 0.11f, 1.0f));
	TitleBar->SetPadding(FMargin(14.0f, 10.0f));
	ShellVBox->AddChild(TitleBar);

	UHorizontalBox* TitleRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(),
		TEXT("TitleRow"));
	TitleBar->AddChild(TitleRow);

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
	SetTextStyle(TitleText, TEXT("Structural Power Ids"), 15, true);
	TitleRow->AddChild(TitleText);
	if (UHorizontalBoxSlot* TitleSlot = Cast<UHorizontalBoxSlot>(TitleText->Slot))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Left);
		TitleSlot->SetVerticalAlignment(VAlign_Center);
		TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	CloseButton = MakeLabeledButton(WidgetTree, TEXT("CloseButton"), TEXT("X"));
	TitleRow->AddChild(CloseButton);
	if (UHorizontalBoxSlot* CloseSlot = Cast<UHorizontalBoxSlot>(CloseButton->Slot))
	{
		CloseSlot->SetHorizontalAlignment(HAlign_Right);
		CloseSlot->SetVerticalAlignment(VAlign_Center);
	}

	CloseButton->OnClicked.Clear();
	CloseButton->OnClicked.AddDynamic(this, &UStructuralPowerIdConfigWidget::OnCloseClicked);

	ContentBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ContentBorder"));
	ContentBorder->SetBrushColor(FLinearColor(0.05f, 0.07f, 0.09f, 0.98f));
	ContentBorder->SetPadding(FMargin(18.0f));

	ContentVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ContentVBox"));
	ContentBorder->AddChild(ContentVBox);
	ShellVBox->AddChild(ContentBorder);

	WidgetTree->RootWidget = RootOverlay;
	SetVisibility(ESlateVisibility::Visible);
	if (TSharedPtr<SWidget> SlateRoot = GetCachedWidget())
	{
		SlateRoot->Invalidate(EInvalidateWidgetReason::Layout);
	}
	UE_LOG(LogStructuralPower, Log, TEXT("[PWR] Id panel shell built root=%s chrome=cpp"), *RootOverlay->GetName());
}

void UStructuralPowerIdConfigWidget::RebuildPanelContent()
{
	EnsureShellBuilt();

	if (!IsValid(ContentVBox) || !IsValid(WidgetTree) || !IsValid(OptionManager) || !IsValid(TargetBuildable))
	{
		return;
	}

	ContentVBox->ClearChildren();
	SuggestedSourceCombo = nullptr;
	SuggestedControlCombo = nullptr;
	AssignSourceText = nullptr;
	AssignControlText = nullptr;
	ApplyButton = nullptr;
	ResetButton = nullptr;
	ActiveIdsText = nullptr;

	FStructuralPowerIdFieldMatrixWidgets BuiltWidgets;
	if (!FStructuralPowerIdFieldMatrix::Build(
			WidgetTree, ContentVBox, TargetBuildable, OptionManager, BuiltWidgets))
	{
		return;
	}

	SuggestedSourceCombo = BuiltWidgets.SuggestedSourceCombo;
	SuggestedControlCombo = BuiltWidgets.SuggestedControlCombo;
	AssignSourceText = BuiltWidgets.AssignSourceText;
	AssignControlText = BuiltWidgets.AssignControlText;
	ApplyButton = BuiltWidgets.ApplyButton;
	ResetButton = BuiltWidgets.ResetButton;
	ActiveIdsText = BuiltWidgets.ActiveIdsText;

	if (IsValid(SuggestedSourceCombo))
	{
		SuggestedSourceCombo->OnSelectionChanged.AddDynamic(
			this,
			&UStructuralPowerIdConfigWidget::OnSuggestedSourceComboChanged);
	}

	if (IsValid(SuggestedControlCombo))
	{
		SuggestedControlCombo->OnSelectionChanged.AddDynamic(
			this,
			&UStructuralPowerIdConfigWidget::OnSuggestedControlComboChanged);
	}

	if (IsValid(ApplyButton))
	{
		ApplyButton->OnClicked.AddDynamic(this, &UStructuralPowerIdConfigWidget::OnApplyTypedIdClicked);
	}

	if (IsValid(ResetButton))
	{
		ResetButton->OnClicked.AddDynamic(this, &UStructuralPowerIdConfigWidget::OnResetIdClicked);
	}

	FlushPendingIdList();
}

void UStructuralPowerIdConfigWidget::ApplySourceIndex(int32 Index)
{
	if (bSuppressServerApply || !IsValid(TargetBuildable) || !IsValid(OptionManager))
	{
		return;
	}

	AFGPlayerController* PC = GetOwningPlayer<AFGPlayerController>();
	FStructuralPowerIdApplyBridge::ApplySourceIndex(PC, TargetBuildable, OptionManager, Index);
}

void UStructuralPowerIdConfigWidget::ApplyControlIndex(int32 Index)
{
	if (bSuppressServerApply || !IsValid(TargetBuildable) || !IsValid(OptionManager))
	{
		return;
	}

	AFGPlayerController* PC = GetOwningPlayer<AFGPlayerController>();
	FStructuralPowerIdApplyBridge::ApplyControlIndex(PC, TargetBuildable, OptionManager, Index);
}

void UStructuralPowerIdConfigWidget::ApplySuggestedIndex(int32 Index)
{
	if (OptionManager->UsesSourceChannel())
	{
		ApplySourceIndex(Index);
	}
	else
	{
		ApplyControlIndex(Index);
	}
}

void UStructuralPowerIdConfigWidget::OnSuggestedSourceComboChanged(
	FString SelectedItem,
	ESelectInfo::Type SelectionType)
{
	if (bSuppressServerApply || SelectionType == ESelectInfo::Direct || !IsValid(OptionManager)
		|| !IsValid(AssignSourceText))
	{
		return;
	}

	const int32 OptionCount = OptionManager->GetSourceOptionCount();
	for (int32 Index = 0; Index < OptionCount; ++Index)
	{
		if (FormatIdOptionLabel(OptionManager->GetSourceOptionAt(Index)) != SelectedItem)
		{
			continue;
		}

		const FName Id = OptionManager->GetSourceOptionAt(Index);
		if (FStructuralPowerRouter::IsPlayerChosenIdValid(Id))
		{
			AssignSourceText->SetText(FText::FromName(Id));
		}
		else
		{
			AssignSourceText->SetText(FText::GetEmpty());
		}

		return;
	}
}

void UStructuralPowerIdConfigWidget::OnSuggestedControlComboChanged(
	FString SelectedItem,
	ESelectInfo::Type SelectionType)
{
	if (bSuppressServerApply || SelectionType == ESelectInfo::Direct || !IsValid(OptionManager)
		|| !IsValid(AssignControlText))
	{
		return;
	}

	const int32 OptionCount = OptionManager->GetControlOptionCount();
	for (int32 Index = 0; Index < OptionCount; ++Index)
	{
		if (FormatIdOptionLabel(OptionManager->GetControlOptionAt(Index)) != SelectedItem)
		{
			continue;
		}

		const FName Id = OptionManager->GetControlOptionAt(Index);
		if (FStructuralPowerRouter::IsPlayerChosenIdValid(Id))
		{
			AssignControlText->SetText(FText::FromName(Id));
		}
		else
		{
			AssignControlText->SetText(FText::GetEmpty());
		}

		return;
	}
}

void UStructuralPowerIdConfigWidget::ApplyTypedIdsToServer()
{
	if (!IsValid(TargetBuildable) || !IsValid(OptionManager))
	{
		return;
	}

	AFGPlayerController* PC = GetOwningPlayer<AFGPlayerController>();
	FStructuralPowerIdApplyBridge::ApplyTypedIds(
		PC,
		TargetBuildable,
		OptionManager,
		SuggestedSourceCombo,
		SuggestedControlCombo,
		AssignSourceText,
		AssignControlText);
}

void UStructuralPowerIdConfigWidget::OnApplyTypedIdClicked()
{
	ApplyTypedIdsToServer();
}

void UStructuralPowerIdConfigWidget::OnResetIdClicked()
{
	if (!IsValid(TargetBuildable))
	{
		return;
	}

	if (AssignSourceText)
	{
		AssignSourceText->SetText(FText::GetEmpty());
	}

	if (AssignControlText)
	{
		AssignControlText->SetText(FText::GetEmpty());
	}

	AFGPlayerController* PC = GetOwningPlayer<AFGPlayerController>();
	const bool bDual = IsValid(OptionManager) && OptionManager->NeedsDualFields();
	FStructuralPowerIdApplyBridge::ResetEndpointIds(PC, TargetBuildable, bDual);
}

void UStructuralPowerIdConfigWidget::RequestIdList()
{
	if (!IsValid(TargetBuildable))
	{
		return;
	}

	if (AFGPlayerController* PC = GetOwningPlayer<AFGPlayerController>())
	{
		FStructuralPowerIdApplyBridge::RequestComponentIdList(PC, TargetBuildable);
	}
}

void UStructuralPowerIdConfigWidget::OnCloseClicked()
{
	ClosePanel();
}

void UStructuralPowerIdConfigWidget::ClosePanel()
{
	if (!bModalActive)
	{
		return;
	}

	if (ActiveInstance.Get() == this)
	{
		SetActiveWidget(nullptr);
	}

	bInitializedForTarget = false;
	TargetBuildable = nullptr;
	bModalActive = false;
	DetachFromViewport();

	if (AFGPlayerController* PC = GetOwningPlayer<AFGPlayerController>())
	{
		FStructuralPowerIdInput::NotifyPanelClosed(PC);
	}

	UE_LOG(LogStructuralPower, Log,
		TEXT("[PWR] Id panel closed inViewport=%d cached=%d"),
		IsInViewport() ? 1 : 0,
		CachedWindow.Get() == this ? 1 : 0);
}
