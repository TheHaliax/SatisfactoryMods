// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Session/FPCLoadAnnouncement.h"

#include "FGChatManager.h"
#include "FGGameState.h"
#include "FGPlayerController.h"
#include "PipelineColorLog.h"
#include "TimerManager.h"

namespace {
FDelegateHandle GLoadAnnounceHandle;
TSet<TWeakObjectPtr<AFGPlayerController>> GPlayersAnnounced;

constexpr const TCHAR* ChatSenderName = TEXT("Hal");

bool ShouldAnnounce(AFGPlayerController* PC, UWorld* World) {
  if (!IsValid(PC) || !IsValid(World)) {
    return false;
  }

  const ENetMode NetMode = World->GetNetMode();
  if (NetMode == NM_Client) {
    return PC->IsLocalController();
  }
  if (NetMode == NM_DedicatedServer && PC->IsLocalController()) {
    return false;
  }
  return PC->GetNetConnection() != nullptr || NetMode == NM_Standalone;
}

void SendSystemChat(AFGPlayerController* PC, const FString& Message) {
  if (!IsValid(PC)) {
    return;
  }

  UWorld* World = PC->GetWorld();
  if (!IsValid(World)) {
    return;
  }

  AGameStateBase* GameState = World->GetGameState();
  if (!GameState) {
    return;
  }

  FChatMessageStruct Msg;
  Msg.MessageText = FText::FromString(Message);
  Msg.MessageType = EFGChatMessageType::CMT_CustomMessage;
  Msg.ServerTimeStamp = GameState->GetServerWorldTimeSeconds();
  Msg.MessageSenderColor = FLinearColor::Green;
  Msg.MessageSender = FText::FromString(ChatSenderName);

  const bool bLocal = World->GetNetMode() == NM_Standalone ||
                      (World->GetNetMode() == NM_Client && PC->IsLocalController());

  if (bLocal) {
    if (AFGChatManager* Chat = AFGChatManager::Get(World)) {
      Chat->AddChatMessageToReceived(Msg);
    }
  } else {
    PC->Client_SendChatMessage(Msg);
  }
}

void Announce(AFGPlayerController* PC) {
  if (!IsValid(PC)) {
    return;
  }

  for (auto It = GPlayersAnnounced.CreateIterator(); It; ++It) {
    if (!It->IsValid()) {
      It.RemoveCurrent();
    }
  }

  if (GPlayersAnnounced.Contains(PC)) {
    return;
  }

  GPlayersAnnounced.Add(PC);
  SendSystemChat(PC, TEXT("Pipeline Color loaded."));
  UE_LOG(LogPipelineColor, Log, TEXT("%s chat banner sent"), PIPELINECOLOR_LOG_PREFIX);
}

void Schedule(AFGPlayerController* PC) {
  if (!IsValid(PC)) {
    return;
  }

  UWorld* World = PC->GetWorld();
  if (!ShouldAnnounce(PC, World) || !IsValid(World)) {
    return;
  }

  FTimerHandle Handle;
  World->GetTimerManager().SetTimer(
      Handle,
      FTimerDelegate::CreateWeakLambda(
          PC, [Weak = TWeakObjectPtr<AFGPlayerController>(PC)]() { Announce(Weak.Get()); }),
      2.0f, false);
}
} // namespace

void FPCLoadAnnouncement::Register() {
  if (GLoadAnnounceHandle.IsValid()) {
    return;
  }

  GLoadAnnounceHandle = AFGPlayerController::PlayerControllerBegunPlay.AddLambda(
      [](AFGPlayerController* PC) { Schedule(PC); });

  UE_LOG(LogPipelineColor, Log, TEXT("%s load announcement hooked"), PIPELINECOLOR_LOG_PREFIX);
}
