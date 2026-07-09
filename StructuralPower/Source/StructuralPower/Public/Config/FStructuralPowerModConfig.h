// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class UWorld;

class STRUCTURALPOWER_API FStructuralPowerModConfig
{
public:
	static constexpr const TCHAR* ModReference = TEXT("StructuralPower");

	static void RegisterConsoleVariables();
	static void LoadRuntimeConfig();
	static void SaveLegacyToDisk();

	static bool CanMutateLiveConfig(UWorld* World);
	static void RequestGroupLightingReconcile(UWorld* World);

	static bool TryApplySetCommand(const TArray<FString>& Args, UWorld* World);

	static bool IsGroupLightingEnabled();
	static float GetHoverpackHorizontalMultiplier();
	static float GetHoverpackVerticalMultiplier();
	static bool IsTraceEnabled();
	static bool IsExtendedDebugEnabled();

	static FString GetConfigFilePath();
};
