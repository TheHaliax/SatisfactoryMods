// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class UConfigPropertySection;
class UGameInstance;

/** Global toggles: SML pause-menu `.cfg` + mirrored CVars + `StructuralPower.Set`. */
class STRUCTURALPOWER_API FStructuralPowerModConfig
{
public:
	static constexpr const TCHAR* ModReference = TEXT("StructuralPower");

	static void RegisterConsoleVariables();
	static void LoadLegacyFromDisk();
	static void SaveLegacyToDisk();

	static void SyncRuntimeFromConfigManager(UGameInstance* GameInstance);
	static void BindLiveConfigHandlers(UGameInstance* GameInstance);
	static void ApplyFromConfigRoot(UConfigPropertySection* Root);
	static void PushToConfigRoot(UConfigPropertySection* Root);

	static bool TryApplySetCommand(const TArray<FString>& Args, UWorld* World);

	static bool IsPropagationEnabled();
	static bool IsGatePowerSwitchesEnabled();
	static bool IsPowerSwitchManualGroupsEnabled();
	static bool IsHoverpackStructuralEnabled();
	static float GetHoverpackHorizontalMultiplier();
	static float GetHoverpackVerticalMultiplier();
	static bool IsTraceEnabled();

	static FString GetConfigFilePath();
};
