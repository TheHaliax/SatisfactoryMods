// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "UI/FStructuralPowerIdShellBuilder.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Rules/FStructuralEligibilityRules.h"
#include "StructuralPowerLog.h"
#include "UI/FStructuralPowerIdWidgetHelpers.h"
#include "UI/UStructuralPowerIdConfigWidget.h"

using StructuralPowerIdUiHelpers::MakeLabeledButton;
using StructuralPowerIdUiHelpers::SetTextStyle;

void FStructuralPowerIdShellBuilder::ResetShellState(
	TObjectPtr<UWidgetTree>& InWidgetTree,
	FStructuralPowerIdShellWidgets& Shell)
{
	Shell = FStructuralPowerIdShellWidgets{};

	if (InWidgetTree)
	{
		InWidgetTree->RootWidget = nullptr;
	}
}

void FStructuralPowerIdShellBuilder::EnsureShellBuilt(
	UUserWidget* Widget,
	TObjectPtr<UWidgetTree>& InWidgetTree,
	FStructuralPowerIdShellWidgets& Shell)
{
	if (!IsValid(Widget))
	{
		return;
	}

	if (IsValid(Shell.ContentVBox) && IsValid(InWidgetTree) && IsValid(InWidgetTree->RootWidget))
	{
		return;
	}

	ResetShellState(InWidgetTree, Shell);

	if (!InWidgetTree)
	{
		InWidgetTree = NewObject<UWidgetTree>(Widget, TEXT("WidgetTree"));
	}

	UOverlay* RootOverlay = InWidgetTree->ConstructWidget<UOverlay>(
		UOverlay::StaticClass(),
		TEXT("RootOverlay"));
	RootOverlay->SetVisibility(ESlateVisibility::Visible);
	RootOverlay->SetClipping(EWidgetClipping::ClipToBoundsAlways);

	UBorder* Backdrop = InWidgetTree->ConstructWidget<UBorder>(
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

	USizeBox* Frame = InWidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(),
		TEXT("RootFrame"));
	Frame->SetWidthOverride(500.0f);
	Frame->SetMinDesiredHeight(380.0f);
	Frame->SetVisibility(ESlateVisibility::Visible);
	RootOverlay->AddChild(Frame);
	if (UOverlaySlot* FrameSlot = Cast<UOverlaySlot>(Frame->Slot))
	{
		FrameSlot->SetHorizontalAlignment(HAlign_Center);
		FrameSlot->SetVerticalAlignment(VAlign_Center);
	}

	Shell.ShellBorder = InWidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("ShellBorder"));
	Shell.ShellBorder->SetBrushColor(FLinearColor(0.03f, 0.05f, 0.07f, 0.98f));
	Shell.ShellBorder->SetPadding(FMargin(0.0f));
	Frame->AddChild(Shell.ShellBorder);

	UVerticalBox* ShellVBox = InWidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(),
		TEXT("ShellVBox"));
	Shell.ShellBorder->AddChild(ShellVBox);

	UBorder* TitleBar = InWidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("TitleBar"));
	TitleBar->SetBrushColor(FLinearColor(0.06f, 0.08f, 0.11f, 1.0f));
	TitleBar->SetPadding(FMargin(14.0f, 10.0f));
	ShellVBox->AddChild(TitleBar);

	UHorizontalBox* TitleRow = InWidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(),
		TEXT("TitleRow"));
	TitleBar->AddChild(TitleRow);

	Shell.TitleText = InWidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("TitleText"));
	SetTextStyle(Shell.TitleText, TEXT("Structural Power Ids"), 15, true);
	TitleRow->AddChild(Shell.TitleText);
	if (UHorizontalBoxSlot* TitleSlot = Cast<UHorizontalBoxSlot>(Shell.TitleText->Slot))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Left);
		TitleSlot->SetVerticalAlignment(VAlign_Center);
		TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	Shell.CloseButton = MakeLabeledButton(InWidgetTree, TEXT("CloseButton"), TEXT("X"));
	TitleRow->AddChild(Shell.CloseButton);
	if (UHorizontalBoxSlot* CloseSlot = Cast<UHorizontalBoxSlot>(Shell.CloseButton->Slot))
	{
		CloseSlot->SetHorizontalAlignment(HAlign_Right);
		CloseSlot->SetVerticalAlignment(VAlign_Center);
	}

	Shell.ContentBorder = InWidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("ContentBorder"));
	Shell.ContentBorder->SetBrushColor(FLinearColor(0.05f, 0.07f, 0.09f, 0.98f));
	Shell.ContentBorder->SetPadding(FMargin(18.0f));

	Shell.ContentVBox = InWidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(),
		TEXT("ContentVBox"));
	Shell.ContentBorder->AddChild(Shell.ContentVBox);
	ShellVBox->AddChild(Shell.ContentBorder);

	InWidgetTree->RootWidget = RootOverlay;
	Widget->SetVisibility(ESlateVisibility::Visible);
	if (TSharedPtr<SWidget> SlateRoot = Widget->GetCachedWidget())
	{
		SlateRoot->Invalidate(EInvalidateWidgetReason::Layout);
	}
	UE_LOG(LogStructuralPower, Log,
		TEXT("[HALSP] Id panel shell built root=%s chrome=cpp"),
		*RootOverlay->GetName());
}

void FStructuralPowerIdShellBuilder::UpdateWindowTitle(
	const FStructuralPowerIdShellWidgets& Shell,
	AFGBuildable* TargetBuildable)
{
	if (!IsValid(Shell.TitleText) || !IsValid(TargetBuildable))
	{
		return;
	}

	const bool bLight = FStructuralEligibilityRules::IsStructuralLightConsumer(TargetBuildable);
	const bool bSwitch = TargetBuildable->IsA<AFGBuildableCircuitSwitch>();
	const TCHAR* Kind = bLight ? TEXT("Light") : (bSwitch ? TEXT("Switch") : TEXT("Panel"));
	SetTextStyle(
		Shell.TitleText,
		FString::Printf(TEXT("Structural Power — %s"), Kind),
		15,
		true);
}

void FStructuralPowerIdShellBuilder::EnsureViewportPresentation(UStructuralPowerIdConfigWidget* Widget)
{
	if (!IsValid(Widget))
	{
		return;
	}

	Widget->SetAnchorsInViewport(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	Widget->SetAlignmentInViewport(FVector2D::ZeroVector);
}
