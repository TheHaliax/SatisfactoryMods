// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Command/FStructuralPowerBangCommands.h"

#include "Config/FStructuralPowerModConfig.h"
#include "FGChatManager.h"
#include "FGGameState.h"
#include "FGPlayerController.h"
#include "Network/UStructuralPowerRCO.h"
#include "Save/AStructuralPowerGraphSubsystem.h"
#include "TimerManager.h"

namespace {
static FDelegateHandle BangChatHookHandle;
static TSet<TWeakObjectPtr<AFGPlayerController>> PlayersWithLoadAnnouncement;

static constexpr const TCHAR* ChatSenderName = TEXT("Hal");

static void SendSystemChat(AFGPlayerController* PlayerController, const FString& Message,
                           const FLinearColor& Color = FLinearColor::Green) {
  if (!IsValid(PlayerController)) {
    return;
  }

  UWorld* World = PlayerController->GetWorld();
  if (!IsValid(World)) {
    return;
  }

  AFGChatManager* ChatManager = AFGChatManager::Get(World);
  AGameStateBase* GameState = World->GetGameState();
  if (!GameState) {
    return;
  }

  FChatMessageStruct MessageStruct;
  MessageStruct.MessageText = FText::FromString(Message);
  MessageStruct.MessageType = EFGChatMessageType::CMT_CustomMessage;
  MessageStruct.ServerTimeStamp = GameState->GetServerWorldTimeSeconds();
  MessageStruct.MessageSenderColor = Color;
  MessageStruct.MessageSender = FText::FromString(ChatSenderName);

  const bool bUseLocalChatBuffer =
      World->GetNetMode() == NM_Standalone ||
      (World->GetNetMode() == NM_Client && PlayerController->IsLocalController());

  if (bUseLocalChatBuffer) {
    if (ChatManager) {
      ChatManager->AddChatMessageToReceived(MessageStruct);
    }
  } else {
    PlayerController->Client_SendChatMessage(MessageStruct);
  }
}

static bool IsStructuralPowerCommand(const FString& CommandLine) {
  TArray<FString> Tokens;
  CommandLine.ParseIntoArrayWS(Tokens);
  if (Tokens.Num() == 0) {
    return false;
  }

  const FString& Verb = Tokens[0];
  return Verb.Equals(TEXT("HoverH"), ESearchCase::IgnoreCase) ||
         Verb.Equals(TEXT("HoverV"), ESearchCase::IgnoreCase) ||
         Verb.Equals(TEXT("lighting"), ESearchCase::IgnoreCase) ||
         Verb.Equals(TEXT("generation"), ESearchCase::IgnoreCase) ||
         Verb.Equals(TEXT("resources"), ESearchCase::IgnoreCase) ||
         Verb.Equals(TEXT("production"), ESearchCase::IgnoreCase) ||
         Verb.Equals(TEXT("transport"), ESearchCase::IgnoreCase) ||
         Verb.Equals(TEXT("pipes"), ESearchCase::IgnoreCase) ||
         Verb.Equals(TEXT("pipe"), ESearchCase::IgnoreCase) ||
         Verb.Equals(TEXT("belts"), ESearchCase::IgnoreCase) ||
         Verb.Equals(TEXT("pwrhelp"), ESearchCase::IgnoreCase);
}

static bool ShouldAnnounceLoadToPlayer(AFGPlayerController* PlayerController, UWorld* World) {
  if (!IsValid(PlayerController) || !IsValid(World)) {
    return false;
  }

  const ENetMode NetMode = World->GetNetMode();

  if (NetMode == NM_Client) {
    return PlayerController->IsLocalController();
  }

  if (NetMode == NM_DedicatedServer && PlayerController->IsLocalController()) {
    return false;
  }

  return PlayerController->GetNetConnection() != nullptr || NetMode == NM_Standalone;
}

static void AnnounceModLoadedForPlayer(AFGPlayerController* PlayerController) {
  if (!IsValid(PlayerController)) {
    return;
  }

  for (auto It = PlayersWithLoadAnnouncement.CreateIterator(); It; ++It) {
    if (!It->IsValid()) {
      It.RemoveCurrent();
    }
  }

  if (PlayersWithLoadAnnouncement.Contains(PlayerController)) {
    return;
  }

  PlayersWithLoadAnnouncement.Add(PlayerController);
  SendSystemChat(PlayerController, TEXT("Structural Power loaded."));
}

static void ScheduleLoadAnnouncement(AFGPlayerController* PlayerController) {
  if (!IsValid(PlayerController)) {
    return;
  }

  UWorld* World = PlayerController->GetWorld();
  if (!ShouldAnnounceLoadToPlayer(PlayerController, World)) {
    return;
  }

  FTimerHandle Handle;
  World->GetTimerManager().SetTimer(
      Handle,
      [PlayerControllerWeak = TWeakObjectPtr<AFGPlayerController>(PlayerController)]() {
        AnnounceModLoadedForPlayer(PlayerControllerWeak.Get());
      },
      2.0f, false);
}

static bool TryToggleGroup(UWorld* World, bool (*IsEnabled)(), const TCHAR* ConfigKey,
                           const TCHAR* OnMessage, const TCHAR* OffMessage, FString& OutMessage) {
  const bool bOn = !IsEnabled();
  const TArray<FString> Args = {ConfigKey, bOn ? TEXT("1") : TEXT("0")};
  if (!FStructuralPowerModConfig::TryApplySetCommand(Args, World)) {
    return false;
  }

  OutMessage = bOn ? OnMessage : OffMessage;
  return true;
}

static bool TrySetHoverMultiplier(UWorld* World, const TCHAR* ConfigKey, const TCHAR* AxisLabel,
                                  float Value, FString& OutMessage) {
  const TArray<FString> Args = {ConfigKey, FString::SanitizeFloat(Value)};
  if (!FStructuralPowerModConfig::TryApplySetCommand(Args, World)) {
    return false;
  }

  OutMessage =
      FString::Printf(TEXT("Hoverpack %s reach multiplier set to %s."), AxisLabel, *Args[1]);
  return true;
}

static void SendHelp(AFGPlayerController* PlayerController) {
  SendSystemChat(PlayerController,
                 TEXT("!HoverH <1-10>  !HoverV <1-10>  — hoverpack reach multipliers"),
                 FLinearColor::Yellow);
  SendSystemChat(PlayerController,
                 TEXT("!lighting !generation !resources !production — group toggles (default off)"),
                 FLinearColor::Yellow);
  SendSystemChat(PlayerController, TEXT("!transport !pipe/!pipes !belts — group toggles (default off)"),
                 FLinearColor::Yellow);
}
}

void FStructuralPowerBangCommands::RegisterChatHook() {
  if (BangChatHookHandle.IsValid()) {
    return;
  }

  BangChatHookHandle = AFGPlayerController::PlayerControllerBegunPlay.AddLambda(
      [](AFGPlayerController* PlayerController) {
        if (!IsValid(PlayerController)) {
          return;
        }

        ScheduleLoadAnnouncement(PlayerController);

        PlayerController->ChatMessageEntered.AddLambda(
            [PlayerControllerWeak = TWeakObjectPtr<AFGPlayerController>(PlayerController)](
                const FString& ChatMessage, bool& bShouldSendMessage) {
              const FString Trimmed = ChatMessage.TrimStartAndEnd();
              if (!Trimmed.StartsWith(TEXT("!"))) {
                return;
              }

              const FString CommandLine = Trimmed.Mid(1);
              if (!IsStructuralPowerCommand(CommandLine)) {
                return;
              }

              AFGPlayerController* PlayerController = PlayerControllerWeak.Get();
              if (!IsValid(PlayerController)) {
                return;
              }

              if (UStructuralPowerRCO* Rco =
                      PlayerController->GetRemoteCallObjectOfClass<UStructuralPowerRCO>()) {
                Rco->Server_RunBangCommand(CommandLine);
                bShouldSendMessage = false;
              }
            });
      });
}

void FStructuralPowerBangCommands::Execute(AFGPlayerController* PlayerController,
                                           const FString& CommandLine) {
  if (!IsValid(PlayerController)) {
    return;
  }

  UWorld* World = PlayerController->GetWorld();
  if (!IsValid(World) || World->GetNetMode() == NM_Client) {
    SendSystemChat(PlayerController, TEXT("Structural Power chat commands run on the server only."),
                   FLinearColor::Red);
    return;
  }

  TArray<FString> Tokens;
  CommandLine.ParseIntoArrayWS(Tokens);
  if (Tokens.Num() == 0) {
    SendHelp(PlayerController);
    return;
  }

  const FString Verb = Tokens[0];
  FString Feedback;

  if (Verb.Equals(TEXT("pwrhelp"), ESearchCase::IgnoreCase)) {
    SendHelp(PlayerController);
    return;
  }

  if (Verb.Equals(TEXT("HoverH"), ESearchCase::IgnoreCase)) {
    if (Tokens.Num() < 2) {
      SendSystemChat(PlayerController, TEXT("Use !HoverH <1-10>"), FLinearColor::Red);
      return;
    }

    if (TrySetHoverMultiplier(World, TEXT("HoverpackStructuralHorizontalMultiplier"),
                              TEXT("horizontal"), FCString::Atof(*Tokens[1]), Feedback)) {
      SendSystemChat(PlayerController, Feedback);
    }
    return;
  }

  if (Verb.Equals(TEXT("HoverV"), ESearchCase::IgnoreCase)) {
    if (Tokens.Num() < 2) {
      SendSystemChat(PlayerController, TEXT("Use !HoverV <1-10>"), FLinearColor::Red);
      return;
    }

    if (TrySetHoverMultiplier(World, TEXT("HoverpackStructuralVerticalMultiplier"),
                              TEXT("vertical"), FCString::Atof(*Tokens[1]), Feedback)) {
      SendSystemChat(PlayerController, Feedback);
    }
    return;
  }

  struct FGroupBang {
    const TCHAR* Verb;
    bool (*IsEnabled)();
    const TCHAR* ConfigKey;
    const TCHAR* OnMessage;
    const TCHAR* OffMessage;
  };

  static const FGroupBang GroupBangs[] = {
      {
          TEXT("lighting"),
          &FStructuralPowerModConfig::IsGroupLightingEnabled,
          TEXT("GroupLighting"),
          TEXT("Structural lighting enabled — unwired lights on powered foundations may draw."),
          TEXT("Structural lighting disabled — lights need vanilla wires."),
      },
      {
          TEXT("generation"),
          &FStructuralPowerModConfig::IsGroupGenerationEnabled,
          TEXT("GroupGeneration"),
          TEXT("Structural generation enabled — gens/storage may attach on powered structure."),
          TEXT("Structural generation disabled — gens/storage need vanilla wires."),
      },
      {
          TEXT("resources"),
          &FStructuralPowerModConfig::IsGroupResourcesEnabled,
          TEXT("GroupResources"),
          TEXT("Structural resources enabled — extractors may attach on powered structure."),
          TEXT("Structural resources disabled — extractors need vanilla wires."),
      },
      {
          TEXT("production"),
          &FStructuralPowerModConfig::IsGroupProductionEnabled,
          TEXT("GroupProduction"),
          TEXT("Structural production enabled — manufacturers may attach on powered structure."),
          TEXT("Structural production disabled — manufacturers need vanilla wires."),
      },
      {
          TEXT("transport"),
          &FStructuralPowerModConfig::IsGroupTransportEnabled,
          TEXT("GroupTransport"),
          TEXT("Structural transport enabled — wired transport consumers may attach."),
          TEXT("Structural transport disabled — transport needs vanilla wires."),
      },
      {
          TEXT("pipes"),
          &FStructuralPowerModConfig::IsGroupPipesEnabled,
          TEXT("GroupPipes"),
          TEXT("Structural pipes enabled — pipe bus via supports/machines; pumps attach."),
          TEXT("Structural pipes disabled — pumps need vanilla wires."),
      },
      {
          TEXT("pipe"),
          &FStructuralPowerModConfig::IsGroupPipesEnabled,
          TEXT("GroupPipes"),
          TEXT("Structural pipes enabled — pipe bus via supports/machines; pumps attach."),
          TEXT("Structural pipes disabled — pumps need vanilla wires."),
      },
      {
          TEXT("belts"),
          &FStructuralPowerModConfig::IsGroupBeltsEnabled,
          TEXT("GroupBelts"),
          TEXT("Structural belts toggle on (no attach yet)."),
          TEXT("Structural belts toggle off."),
      },
  };

  for (const FGroupBang& Bang : GroupBangs) {
    if (!Verb.Equals(Bang.Verb, ESearchCase::IgnoreCase)) {
      continue;
    }

    if (TryToggleGroup(World, Bang.IsEnabled, Bang.ConfigKey, Bang.OnMessage, Bang.OffMessage,
                       Feedback)) {
      SendSystemChat(PlayerController, Feedback);
    }
    return;
  }

  SendSystemChat(
      PlayerController,
      FString::Printf(TEXT("Unknown Structural Power command: !%s (try !pwrhelp)"), *Verb),
      FLinearColor::Red);
}
