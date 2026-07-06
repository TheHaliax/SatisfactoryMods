// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "FGOptionInterfaceImpl.h"
#include "Routing/EStructuralChannel.h"
#include "UStructuralPowerIdOptionManager.generated.h"

class AFGBuildable;
class UFGUserSetting;
class UFGUserSettingApplyType;
class UUserWidget;

/** Runtime option manager for Structural Power id panel rows (game options generator pipeline). */
UCLASS()
class STRUCTURALPOWER_API UStructuralPowerIdOptionManager : public UObject, public IFGOptionInterfaceImpl
{
	GENERATED_BODY()

public:
	void ConfigureForTarget(AFGBuildable* Target, EStructuralChannel Channel);
	void SyncFromComponentList(const FStructuralComponentIdList& List);

	FName GetSourceIdFromIndex(int32 Index) const;
	FName GetControlIdFromIndex(int32 Index) const;

	// IFGOptionInterfaceImpl
	virtual void GetAllUserSettings(TArray<TObjectPtr<UFGUserSettingApplyType>>& OutUserSettings) const override;
	virtual void GetAllUserSettingsMap(
		TMap<FString, TObjectPtr<UFGUserSettingApplyType>>& OutUserSettings) const override;
	virtual UFGUserSettingApplyType* FindUserSetting(const FString& SettingId) const override;
	virtual IFGOptionInterface* GetPrimaryOptionInterface(UWorld* World) const override;
	virtual bool IsInMainMenu() const override { return false; }
	virtual TArray<FUserSettingCategoryMapping> GetCategorizedSettingWidgets(
		UObject* WorldContext,
		UUserWidget* OwningWidget) override;

	int32 GetSuggestedOptionCount() const;
	FName GetSuggestedOptionAt(int32 Index) const;
	int32 FindSuggestedOptionIndex(FName Id, int32 Fallback = 0) const;
	bool UsesSourceChannel() const;
	bool NeedsDualFields() const;

	int32 GetSourceOptionCount() const;
	int32 GetControlOptionCount() const;
	FName GetSourceOptionAt(int32 Index) const;
	FName GetControlOptionAt(int32 Index) const;
	int32 FindSourceOptionIndex(FName Id, int32 Fallback = 0) const;
	int32 FindControlOptionIndex(FName Id, int32 Fallback = 0) const;

private:
	static constexpr TCHAR ControlSettingId[] = TEXT("StructuralPower.ControlId");

	void ResetSettings();
	UFGUserSetting* CreateDropdownSetting(
		const FString& SettingId,
		const FText& Label,
		const TArray<FName>& Options,
		int32 DefaultIndex);
	void RegisterSetting(UFGUserSetting* Setting);
	void PopulateSelector(UFGUserSetting* Setting, const TArray<FName>& Options, int32 SelectedIndex);

	static constexpr TCHAR SourceSettingId[] = TEXT("StructuralPower.SourceId");

	UPROPERTY(Transient)
	TObjectPtr<AFGBuildable> TargetBuildable;

	EStructuralChannel TargetChannel = EStructuralChannel::Structure;

	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UFGUserSettingApplyType>> Settings;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UFGUserSetting>> OwnedSettings;

	TArray<FName> SourceOptions;
	TArray<FName> ControlOptions;
};
