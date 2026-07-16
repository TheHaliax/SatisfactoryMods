// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class UWorld;

DECLARE_MULTICAST_DELEGATE(FPCPipelineColorConfigChanged);

class PIPELINECOLOR_API FPCPipelineColorModConfig {
 public:
  static constexpr const TCHAR* ModReference = TEXT("PipelineColor");

  static void RegisterConsoleVariables();
  static void LoadRuntimeConfig();
  static void SaveToDisk();

  static bool CanMutateLiveConfig(UWorld* World);
  static bool TryApplySetCommand(const TArray<FString>& Args, UWorld* World);

  static bool IsGasCatalogKey(FName CatalogKey);
  static bool IsMetallicForKey(FName CatalogKey);
  static bool TrySetMetallicOverride(FName CatalogKey, bool bOn, UWorld* World);
  static bool TrySetAllMetallic(bool bOn, UWorld* World);
  static bool TryResetMetallicToDefaults(UWorld* World);

  static bool IsDefaultGasMetallic();
  static bool IsDefaultLiquidMetallic();

  static void ApplyDefaultFlags(bool bGasMetallic, bool bLiquidMetallic);

  static FString GetConfigFilePath();
  static FPCPipelineColorConfigChanged& OnConfigChanged();
};
