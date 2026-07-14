// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Command/StructuralPowerSmlChatCommands.h"

#include "Command/ChatCommandLibrary.h"
#include "Command/CommandSender.h"
#include "Command/FStructuralPowerBangCommands.h"
#include "Config/FStructuralPowerModConfig.h"
#include "FGPlayerController.h"

namespace
{
bool RunBangFromSender(UCommandSender* Sender, const FString& CommandLine)
{
	if (!Sender || !Sender->IsPlayerSender())
	{
		return false;
	}

	AFGPlayerController* Player = Sender->GetPlayer();
	if (!IsValid(Player))
	{
		return false;
	}

	FStructuralPowerBangCommands::Execute(Player, CommandLine);
	return true;
}
} // namespace

void FStructuralPowerSmlChatCommands::RegisterWithWorld(UWorld* World)
{
	if (!IsValid(World) || World->GetNetMode() == NM_Client)
	{
		return;
	}

	AChatCommandSubsystem* Chat = AChatCommandSubsystem::Get(World);
	if (!IsValid(Chat))
	{
		return;
	}

	const FString ModRef = FStructuralPowerModConfig::ModReference;
	Chat->RegisterCommand(ModRef, AStructuralPowerLightingChatCommand::StaticClass());
	Chat->RegisterCommand(ModRef, AStructuralPowerHoverHChatCommand::StaticClass());
	Chat->RegisterCommand(ModRef, AStructuralPowerHoverVChatCommand::StaticClass());
	Chat->RegisterCommand(ModRef, AStructuralPowerPwrHelpChatCommand::StaticClass());
}

AStructuralPowerLightingChatCommand::AStructuralPowerLightingChatCommand()
{
	bOnlyUsableByPlayer = true;
	CommandName = TEXT("lighting");
	Usage = NSLOCTEXT(
		"StructuralPower",
		"ChatCmd.Lighting",
		"!lighting — toggle structural lighting on powered foundations");
}

EExecutionStatus AStructuralPowerLightingChatCommand::ExecuteCommand_Implementation(
	UCommandSender* Sender,
	const TArray<FString>& /*Arguments*/,
	const FString& /*Label*/)
{
	return RunBangFromSender(Sender, TEXT("lighting"))
		? EExecutionStatus::COMPLETED
		: EExecutionStatus::UNCOMPLETED;
}

AStructuralPowerHoverHChatCommand::AStructuralPowerHoverHChatCommand()
{
	bOnlyUsableByPlayer = true;
	MinNumberOfArguments = 1;
	CommandName = TEXT("HoverH");
	Usage = NSLOCTEXT(
		"StructuralPower",
		"ChatCmd.HoverH",
		"!HoverH <1-10> — hoverpack horizontal reach multiplier");
}

EExecutionStatus AStructuralPowerHoverHChatCommand::ExecuteCommand_Implementation(
	UCommandSender* Sender,
	const TArray<FString>& Arguments,
	const FString& /*Label*/)
{
	if (Arguments.Num() < 1)
	{
		PrintCommandUsage(Sender);
		return EExecutionStatus::BAD_ARGUMENTS;
	}

	const FString CommandLine = FString::Printf(TEXT("HoverH %s"), *Arguments[0]);
	return RunBangFromSender(Sender, CommandLine)
		? EExecutionStatus::COMPLETED
		: EExecutionStatus::UNCOMPLETED;
}

AStructuralPowerHoverVChatCommand::AStructuralPowerHoverVChatCommand()
{
	bOnlyUsableByPlayer = true;
	MinNumberOfArguments = 1;
	CommandName = TEXT("HoverV");
	Usage = NSLOCTEXT(
		"StructuralPower",
		"ChatCmd.HoverV",
		"!HoverV <1-10> — hoverpack vertical reach multiplier");
}

EExecutionStatus AStructuralPowerHoverVChatCommand::ExecuteCommand_Implementation(
	UCommandSender* Sender,
	const TArray<FString>& Arguments,
	const FString& /*Label*/)
{
	if (Arguments.Num() < 1)
	{
		PrintCommandUsage(Sender);
		return EExecutionStatus::BAD_ARGUMENTS;
	}

	const FString CommandLine = FString::Printf(TEXT("HoverV %s"), *Arguments[0]);
	return RunBangFromSender(Sender, CommandLine)
		? EExecutionStatus::COMPLETED
		: EExecutionStatus::UNCOMPLETED;
}

AStructuralPowerPwrHelpChatCommand::AStructuralPowerPwrHelpChatCommand()
{
	bOnlyUsableByPlayer = true;
	CommandName = TEXT("pwrhelp");
	Usage = NSLOCTEXT(
		"StructuralPower",
		"ChatCmd.PwrHelp",
		"!pwrhelp — list Structural Power chat commands");
	Aliases.Add(TEXT("sphelp"));
}

EExecutionStatus AStructuralPowerPwrHelpChatCommand::ExecuteCommand_Implementation(
	UCommandSender* Sender,
	const TArray<FString>& /*Arguments*/,
	const FString& /*Label*/)
{
	return RunBangFromSender(Sender, TEXT("pwrhelp"))
		? EExecutionStatus::COMPLETED
		: EExecutionStatus::UNCOMPLETED;
}
