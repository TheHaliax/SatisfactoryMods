// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Command/StructuralPowerSmlChatCommands.h"

#include "Command/ChatCommandLibrary.h"
#include "Command/CommandSender.h"
#include "Command/FStructuralPowerBangCommands.h"
#include "Config/FStructuralPowerModConfig.h"
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

  FStructuralPowerBangCommands::Execute(Player, CommandLine);
  return true;
}

EExecutionStatus RunGroupToggle(UCommandSender* Sender, const TCHAR* Verb) {
  return RunBangFromSender(Sender, Verb) ? EExecutionStatus::COMPLETED
                                         : EExecutionStatus::UNCOMPLETED;
}
} // namespace

void FStructuralPowerSmlChatCommands::RegisterWithWorld(UWorld* World) {
  if (!IsValid(World) || World->GetNetMode() == NM_Client) {
    return;
  }

  AChatCommandSubsystem* Chat = AChatCommandSubsystem::Get(World);
  if (!IsValid(Chat)) {
    return;
  }

  const FString ModRef = FStructuralPowerModConfig::ModReference;
  Chat->RegisterCommand(ModRef, AStructuralPowerLightingChatCommand::StaticClass());
  Chat->RegisterCommand(ModRef, AStructuralPowerGenerationChatCommand::StaticClass());
  Chat->RegisterCommand(ModRef, AStructuralPowerResourcesChatCommand::StaticClass());
  Chat->RegisterCommand(ModRef, AStructuralPowerProductionChatCommand::StaticClass());
  Chat->RegisterCommand(ModRef, AStructuralPowerTransportChatCommand::StaticClass());
  Chat->RegisterCommand(ModRef, AStructuralPowerPipesChatCommand::StaticClass());
  Chat->RegisterCommand(ModRef, AStructuralPowerPipeChatCommand::StaticClass());
  Chat->RegisterCommand(ModRef, AStructuralPowerBeltsChatCommand::StaticClass());
  Chat->RegisterCommand(ModRef, AStructuralPowerHoverHChatCommand::StaticClass());
  Chat->RegisterCommand(ModRef, AStructuralPowerHoverVChatCommand::StaticClass());
  Chat->RegisterCommand(ModRef, AStructuralPowerPwrHelpChatCommand::StaticClass());
}

AStructuralPowerLightingChatCommand::AStructuralPowerLightingChatCommand() {
  bOnlyUsableByPlayer = true;
  CommandName = TEXT("lighting");
  Usage = NSLOCTEXT("StructuralPower", "ChatCmd.Lighting",
                    "!lighting — toggle structural lighting on powered foundations");
}

EExecutionStatus AStructuralPowerLightingChatCommand::ExecuteCommand_Implementation(
    UCommandSender* Sender, const TArray<FString>& /*Arguments*/, const FString& /*Label*/) {
  return RunGroupToggle(Sender, TEXT("lighting"));
}

AStructuralPowerGenerationChatCommand::AStructuralPowerGenerationChatCommand() {
  bOnlyUsableByPlayer = true;
  CommandName = TEXT("generation");
  Usage = NSLOCTEXT("StructuralPower", "ChatCmd.Generation",
                    "!generation — toggle structural generation on powered foundations");
}

EExecutionStatus AStructuralPowerGenerationChatCommand::ExecuteCommand_Implementation(
    UCommandSender* Sender, const TArray<FString>& /*Arguments*/, const FString& /*Label*/) {
  return RunGroupToggle(Sender, TEXT("generation"));
}

AStructuralPowerResourcesChatCommand::AStructuralPowerResourcesChatCommand() {
  bOnlyUsableByPlayer = true;
  CommandName = TEXT("resources");
  Usage = NSLOCTEXT("StructuralPower", "ChatCmd.Resources",
                    "!resources — toggle structural resource extractors");
}

EExecutionStatus AStructuralPowerResourcesChatCommand::ExecuteCommand_Implementation(
    UCommandSender* Sender, const TArray<FString>& /*Arguments*/, const FString& /*Label*/) {
  return RunGroupToggle(Sender, TEXT("resources"));
}

AStructuralPowerProductionChatCommand::AStructuralPowerProductionChatCommand() {
  bOnlyUsableByPlayer = true;
  CommandName = TEXT("production");
  Usage = NSLOCTEXT("StructuralPower", "ChatCmd.Production",
                    "!production — toggle structural manufacturers / production");
}

EExecutionStatus AStructuralPowerProductionChatCommand::ExecuteCommand_Implementation(
    UCommandSender* Sender, const TArray<FString>& /*Arguments*/, const FString& /*Label*/) {
  return RunGroupToggle(Sender, TEXT("production"));
}

AStructuralPowerTransportChatCommand::AStructuralPowerTransportChatCommand() {
  bOnlyUsableByPlayer = true;
  CommandName = TEXT("transport");
  Usage = NSLOCTEXT("StructuralPower", "ChatCmd.Transport",
                    "!transport — toggle structural transport consumers");
}

EExecutionStatus AStructuralPowerTransportChatCommand::ExecuteCommand_Implementation(
    UCommandSender* Sender, const TArray<FString>& /*Arguments*/, const FString& /*Label*/) {
  return RunGroupToggle(Sender, TEXT("transport"));
}

AStructuralPowerPipesChatCommand::AStructuralPowerPipesChatCommand() {
  bOnlyUsableByPlayer = true;
  CommandName = TEXT("pipes");
  Usage = NSLOCTEXT("StructuralPower", "ChatCmd.Pipes",
                    "!pipes - toggle structural pipe bus (supports/machines -> pumps)");
}

EExecutionStatus AStructuralPowerPipesChatCommand::ExecuteCommand_Implementation(
    UCommandSender* Sender, const TArray<FString>& /*Arguments*/, const FString& /*Label*/) {
  return RunGroupToggle(Sender, TEXT("pipes"));
}

AStructuralPowerPipeChatCommand::AStructuralPowerPipeChatCommand() {
  bOnlyUsableByPlayer = true;
  CommandName = TEXT("pipe");
  Usage = NSLOCTEXT("StructuralPower", "ChatCmd.Pipe",
                    "!pipe - alias for !pipes (structural pipe bus)");
}

EExecutionStatus AStructuralPowerPipeChatCommand::ExecuteCommand_Implementation(
    UCommandSender* Sender, const TArray<FString>& /*Arguments*/, const FString& /*Label*/) {
  return RunGroupToggle(Sender, TEXT("pipe"));
}

AStructuralPowerBeltsChatCommand::AStructuralPowerBeltsChatCommand() {
  bOnlyUsableByPlayer = true;
  CommandName = TEXT("belts");
  Usage = NSLOCTEXT("StructuralPower", "ChatCmd.Belts",
                    "!belts — toggle structural belts (no attach yet)");
}

EExecutionStatus AStructuralPowerBeltsChatCommand::ExecuteCommand_Implementation(
    UCommandSender* Sender, const TArray<FString>& /*Arguments*/, const FString& /*Label*/) {
  return RunGroupToggle(Sender, TEXT("belts"));
}

AStructuralPowerHoverHChatCommand::AStructuralPowerHoverHChatCommand() {
  bOnlyUsableByPlayer = true;
  MinNumberOfArguments = 1;
  CommandName = TEXT("HoverH");
  Usage = NSLOCTEXT("StructuralPower", "ChatCmd.HoverH",
                    "!HoverH <1-10> — hoverpack horizontal reach multiplier");
}

EExecutionStatus AStructuralPowerHoverHChatCommand::ExecuteCommand_Implementation(
    UCommandSender* Sender, const TArray<FString>& Arguments, const FString& /*Label*/) {
  if (Arguments.Num() < 1) {
    PrintCommandUsage(Sender);
    return EExecutionStatus::BAD_ARGUMENTS;
  }

  const FString CommandLine = FString::Printf(TEXT("HoverH %s"), *Arguments[0]);
  return RunBangFromSender(Sender, CommandLine) ? EExecutionStatus::COMPLETED
                                                : EExecutionStatus::UNCOMPLETED;
}

AStructuralPowerHoverVChatCommand::AStructuralPowerHoverVChatCommand() {
  bOnlyUsableByPlayer = true;
  MinNumberOfArguments = 1;
  CommandName = TEXT("HoverV");
  Usage = NSLOCTEXT("StructuralPower", "ChatCmd.HoverV",
                    "!HoverV <1-10> — hoverpack vertical reach multiplier");
}

EExecutionStatus AStructuralPowerHoverVChatCommand::ExecuteCommand_Implementation(
    UCommandSender* Sender, const TArray<FString>& Arguments, const FString& /*Label*/) {
  if (Arguments.Num() < 1) {
    PrintCommandUsage(Sender);
    return EExecutionStatus::BAD_ARGUMENTS;
  }

  const FString CommandLine = FString::Printf(TEXT("HoverV %s"), *Arguments[0]);
  return RunBangFromSender(Sender, CommandLine) ? EExecutionStatus::COMPLETED
                                                : EExecutionStatus::UNCOMPLETED;
}

AStructuralPowerPwrHelpChatCommand::AStructuralPowerPwrHelpChatCommand() {
  bOnlyUsableByPlayer = true;
  CommandName = TEXT("pwrhelp");
  Usage = NSLOCTEXT("StructuralPower", "ChatCmd.PwrHelp",
                    "!pwrhelp — list Structural Power chat commands");
  Aliases.Add(TEXT("sphelp"));
}

EExecutionStatus AStructuralPowerPwrHelpChatCommand::ExecuteCommand_Implementation(
    UCommandSender* Sender, const TArray<FString>& /*Arguments*/, const FString& /*Label*/) {
  return RunBangFromSender(Sender, TEXT("pwrhelp")) ? EExecutionStatus::COMPLETED
                                                    : EExecutionStatus::UNCOMPLETED;
}
