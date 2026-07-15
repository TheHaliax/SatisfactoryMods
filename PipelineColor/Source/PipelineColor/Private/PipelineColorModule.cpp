// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "PipelineColorModule.h"
#include "PipelineColorRootInstanceModule.h"

#include "Config/FPCPipelineColorModConfig.h"
#include "Command/FPCBangCommands.h"
#include "HAL/IConsoleManager.h"

#define LOCTEXT_NAMESPACE "FPipelineColorModule"

static FAutoConsoleCommandWithWorldAndArgs GPipelineColorSetCmd(
	TEXT("PipelineColor.Set"),
	TEXT("Set a mod config key and persist to .cfg (authority only)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
		[](const TArray<FString>& Args, UWorld* World)
		{
			FPCPipelineColorModConfig::TryApplySetCommand(Args, World);
		}));

void FPipelineColorModule::StartupModule()
{
	FPCPipelineColorModConfig::RegisterConsoleVariables();
	FPCBangCommands::RegisterChatHook();
}

void FPipelineColorModule::ShutdownModule()
{
	UPipelineColorRootInstanceModule::UnregisterGlobalDelegates();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPipelineColorModule, PipelineColor)
