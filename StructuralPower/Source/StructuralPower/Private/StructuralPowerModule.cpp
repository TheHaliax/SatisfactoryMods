// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "StructuralPowerModule.h"
#include "StructuralPowerRootInstanceModule.h"

#include "Diagnostics/FStructuralPowerDiagnostics.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Save/AStructuralPowerGraphSubsystem.h"
#include "Session/FStructuralPowerSessionSettings.h"

#define LOCTEXT_NAMESPACE "FStructuralPowerModule"

static FAutoConsoleCommandWithWorld GStructuralPowerDiagCmd(
	TEXT("StructuralPower.Diag"),
	TEXT("Audit mod-managed wall outlets (links + circuit IDs)."),
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
				FStructuralPowerDiagnostics::AuditWorld(World);
			}
		}
	}));

void FStructuralPowerModule::StartupModule()
{
	FStructuralPowerSessionSettings::IsPropagationEnabled();
}

void FStructuralPowerModule::ShutdownModule()
{
	UStructuralPowerRootInstanceModule::UnregisterGlobalDelegates();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FStructuralPowerModule, StructuralPower)
