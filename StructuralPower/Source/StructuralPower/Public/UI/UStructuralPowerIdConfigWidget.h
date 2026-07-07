// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Routing/EStructuralChannel.h"
#include "UI/FStructuralPowerIdShellBuilder.h"
#include "UI/FGUserWidget.h"
#include "UStructuralPowerIdConfigWidget.generated.h"

class AFGBuildable;
class FStructuralPowerIdDisplaySync;
class FStructuralPowerIdModalHost;
class FStructuralPowerIdWindowPool;
class UBorder;
class UButton;
class UComboBoxString;
class UEditableTextBox;
class UStructuralPowerIdOptionManager;
class UTextBlock;
class UVerticalBox;

UCLASS()
class STRUCTURALPOWER_API UStructuralPowerIdConfigWidget : public UFGUserWidget
{
	GENERATED_BODY()

	friend class FStructuralPowerIdDisplaySync;
	friend class FStructuralPowerIdModalHost;
	friend class FStructuralPowerIdWindowPool;

public:
	UStructuralPowerIdConfigWidget(const FObjectInitializer& ObjectInitializer);

	void OpenForTarget(AFGBuildable* Target);
	void RetargetTo(AFGBuildable* Target);
	void ApplyComponentIdList(const FStructuralComponentIdList& List);
	void ClosePanel();
	void NormalizeModalState();

	AFGBuildable* GetTargetBuildable() const { return TargetBuildable; }
	bool IsPanelVisible() const;
	bool IsPanelShown() const;
	bool IsTextFieldFocused() const;

	static UStructuralPowerIdConfigWidget* GetActiveWidget();
	static UStructuralPowerIdConfigWidget* GetPooledWindow();
	static UStructuralPowerIdConfigWidget* GetOrCreateWindow(AFGPlayerController* PC);
	static void CloseActivePanel();
	static void ForceReleaseAllModalState(
		AFGPlayerController* PC,
		bool bRestoreGameInputMode = true);
	static void ReleaseForVanillaInteract(AFGPlayerController* PC);
	static void ResetStaticsForMapLoad();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewKeyDown(
		const FGeometry& InGeometry,
		const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnKeyChar(
		const FGeometry& InGeometry,
		const FCharacterEvent& InCharacterEvent) override;

private:
	FStructuralPowerIdShellWidgets BuildShellWidgetRefs() const;
	void ApplyShellWidgetRefs(const FStructuralPowerIdShellWidgets& Shell);

	void EnsureShellBuilt();
	void InitializeForTarget(AFGBuildable* Target);
	void RebuildPanelContent();
	void ClearHotkeyTextBleed();
	void ApplySourceIndex(int32 Index);
	void ApplyControlIndex(int32 Index);
	void ApplySuggestedIndex(int32 Index);
	void ApplyTypedIdsToServer();
	void RequestIdList();

	UFUNCTION()
	void OnCloseClicked();

	UFUNCTION()
	void OnSuggestedSourceComboChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void OnSuggestedControlComboChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void OnApplyTypedIdClicked();

	UFUNCTION()
	void OnResetIdClicked();

	UPROPERTY(Transient)
	TObjectPtr<UStructuralPowerIdOptionManager> OptionManager;

	UPROPERTY(Transient)
	TObjectPtr<AFGBuildable> TargetBuildable;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> ShellBorder;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> CloseButton;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> ContentBorder;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> ContentVBox;

	UPROPERTY(Transient)
	TObjectPtr<UComboBoxString> SuggestedSourceCombo;

	UPROPERTY(Transient)
	TObjectPtr<UComboBoxString> SuggestedControlCombo;

	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> AssignSourceText;

	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> AssignControlText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ApplyButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ResetButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ActiveIdsText;

	bool bSuppressServerApply = false;
	bool bInitializedForTarget = false;
	bool bModalActive = false;
	bool bHasPendingIdList = false;
	bool bSuppressHotkeyChar = false;
	FStructuralComponentIdList PendingIdList;

	static constexpr int32 ViewportZOrder = 10000;
};
