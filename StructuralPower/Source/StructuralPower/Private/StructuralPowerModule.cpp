// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "StructuralPowerModule.h"
#include "StructuralPowerRootInstanceModule.h"
#include "Subsystems/UStructuralPowerFactoryTickHandler.h"

#include "Config/FStructuralPowerModConfig.h"
#include "Command/FStructuralPowerBangCommands.h"
#include "Diagnostics/FStructuralPowerDiagnostics.h"
#include "Save/AStructuralPowerGraphSubsystem.h"

#define LOCTEXT_NAMESPACE "FStructuralPowerModule"

static FAutoConsoleCommandWithWorld GStructuralPowerDiagCmd(
	TEXT("StructuralPower.Diag"),
	TEXT("Audit the structural graph and bridge-pole circuit state."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (IsValid(World))
		{
			if (AStructuralPowerGraphSubsystem* Graph = AStructuralPowerGraphSubsystem::GetOrCreate(World))
			{
				Graph->RunDiagnostics();
			}
			else
			{
				FStructuralPowerDiagnostics::AuditWorld(World, true);
			}
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GStructuralPowerSetCmd(
	TEXT("StructuralPower.Set"),
	TEXT("Set a mod config key and persist to .cfg (authority only)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		FStructuralPowerModConfig::TryApplySetCommand(Args, World);
	}));

void FStructuralPowerModule::StartupModule()
{
	FStructuralPowerModConfig::RegisterConsoleVariables();
	FStructuralPowerBangCommands::RegisterChatHook();
}

void FStructuralPowerModule::ShutdownModule()
{
	UStructuralPowerFactoryTickHandler::UnregisterGlobalDelegates();
	UStructuralPowerRootInstanceModule::UnregisterGlobalDelegates();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FStructuralPowerModule, StructuralPower)
