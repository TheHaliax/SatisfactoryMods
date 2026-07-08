// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class UConfigPropertySection;
class UGameInstance;

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

	static bool IsGroupLightingEnabled();
	static float GetHoverpackHorizontalMultiplier();
	static float GetHoverpackVerticalMultiplier();
	static bool IsTraceEnabled();

	static FString GetConfigFilePath();
};
