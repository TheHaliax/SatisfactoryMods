// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Command/PipelineColorSmlChatCommands.h"

#include "Command/ChatCommandLibrary.h"
#include "Command/CommandSender.h"
#include "Command/FPCBangCommands.h"
#include "Config/FPCPipelineColorModConfig.h"
#include "FGPlayerController.h"

namespace {
bool RunBangFromSender(UCommandSender* Sender, const FString& CommandLine) {
  if (!Sender || !Sender->IsPlayerSender()) {
    return false;
  }

  AFGPlayerController* Player = Sender->GetPlayer();
  if (!IsValid(Player)) {
    return false;
  }

  FPCBangCommands::Execute(Player, CommandLine);
  return true;
}
} // namespace

void FPipelineColorSmlChatCommands::RegisterWithWorld(UWorld* World) {
  if (!IsValid(World) || World->GetNetMode() == NM_Client) {
    return;
  }

  AChatCommandSubsystem* Chat = AChatCommandSubsystem::Get(World);
  if (!IsValid(Chat)) {
    return;
  }

  const FString ModRef = FPCPipelineColorModConfig::ModReference;
  Chat->RegisterCommand(ModRef, APCMetallicChatCommand::StaticClass());
  Chat->RegisterCommand(ModRef, APCPcChatCommand::StaticClass());
  Chat->RegisterCommand(ModRef, APCPchelpChatCommand::StaticClass());
}

APCMetallicChatCommand::APCMetallicChatCommand() {
  bOnlyUsableByPlayer = true;
  MinNumberOfArguments = 1;
  CommandName = TEXT("Metallic");
  Usage = NSLOCTEXT("PipelineColor", "ChatCmd.Metallic",
                    "!Metallic <fluid> | all on | all off | default");
}

EExecutionStatus APCMetallicChatCommand::ExecuteCommand_Implementation(
    UCommandSender* Sender, const TArray<FString>& Arguments, const FString& /*Label*/) {
  if (Arguments.Num() < 1) {
    PrintCommandUsage(Sender);
    return EExecutionStatus::BAD_ARGUMENTS;
  }

  const FString FluidQuery = FString::Join(Arguments, TEXT(" "));
  const FString CommandLine = FString::Printf(TEXT("Metallic %s"), *FluidQuery);
  return RunBangFromSender(Sender, CommandLine) ? EExecutionStatus::COMPLETED
                                                : EExecutionStatus::UNCOMPLETED;
}

APCPcChatCommand::APCPcChatCommand() {
  bOnlyUsableByPlayer = true;
  MinNumberOfArguments = 1;
  CommandName = TEXT("pc");
  Usage = NSLOCTEXT("PipelineColor", "ChatCmd.Pc",
                    "!pc default — reseed swatch colors from fluid data");
}

EExecutionStatus APCPcChatCommand::ExecuteCommand_Implementation(UCommandSender* Sender,
                                                                 const TArray<FString>& Arguments,
                                                                 const FString& /*Label*/) {
  if (Arguments.Num() < 1) {
    PrintCommandUsage(Sender);
    return EExecutionStatus::BAD_ARGUMENTS;
  }

  const FString CommandLine = FString::Printf(TEXT("pc %s"), *FString::Join(Arguments, TEXT(" ")));
  return RunBangFromSender(Sender, CommandLine) ? EExecutionStatus::COMPLETED
                                                : EExecutionStatus::UNCOMPLETED;
}

APCPchelpChatCommand::APCPchelpChatCommand() {
  bOnlyUsableByPlayer = true;
  CommandName = TEXT("pchelp");
  Usage =
      NSLOCTEXT("PipelineColor", "ChatCmd.PcHelp", "!pchelp — list Pipeline Color chat commands");
}

EExecutionStatus APCPchelpChatCommand::ExecuteCommand_Implementation(
    UCommandSender* Sender, const TArray<FString>& /*Arguments*/, const FString& /*Label*/) {
  return RunBangFromSender(Sender, TEXT("pchelp")) ? EExecutionStatus::COMPLETED
                                                   : EExecutionStatus::UNCOMPLETED;
}
