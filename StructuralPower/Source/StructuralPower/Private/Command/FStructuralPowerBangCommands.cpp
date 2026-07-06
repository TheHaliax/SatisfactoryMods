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

namespace
{
static FDelegateHandle BangChatHookHandle;
static TSet<TWeakObjectPtr<AFGPlayerController>> PlayersWithLoadAnnouncement;

static constexpr const TCHAR* ChatSenderName = TEXT("Hal");

static void SendSystemChat(
	AFGPlayerController* PlayerController,
	const FString& Message,
	const FLinearColor& Color = FLinearColor::Green)
{
	if (!IsValid(PlayerController))
	{
		return;
	}

	UWorld* World = PlayerController->GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	AFGChatManager* ChatManager = AFGChatManager::Get(World);
	AGameStateBase* GameState = World->GetGameState();
	if (!GameState)
	{
		return;
	}

	FChatMessageStruct MessageStruct;
	MessageStruct.MessageText = FText::FromString(Message);
	MessageStruct.MessageType = EFGChatMessageType::CMT_CustomMessage;
	MessageStruct.ServerTimeStamp = GameState->GetServerWorldTimeSeconds();
	MessageStruct.MessageSenderColor = Color;
	MessageStruct.MessageSender = FText::FromString(ChatSenderName);

	// Server command replies: authority calls Client_SendChatMessage (dedicated/listen).
	// Load banner + solo: local received buffer on client or standalone.
	const bool bUseLocalChatBuffer = World->GetNetMode() == NM_Standalone
		|| (World->GetNetMode() == NM_Client && PlayerController->IsLocalController());

	if (bUseLocalChatBuffer)
	{
		if (ChatManager)
		{
			ChatManager->AddChatMessageToReceived(MessageStruct);
		}
	}
	else
	{
		PlayerController->Client_SendChatMessage(MessageStruct);
	}
}

static bool IsStructuralPowerCommand(const FString& CommandLine)
{
	TArray<FString> Tokens;
	CommandLine.ParseIntoArrayWS(Tokens);
	if (Tokens.Num() == 0)
	{
		return false;
	}

	const FString& Verb = Tokens[0];
	return Verb.Equals(TEXT("HoverH"), ESearchCase::IgnoreCase)
		|| Verb.Equals(TEXT("HoverV"), ESearchCase::IgnoreCase)
		|| Verb.Equals(TEXT("tracetoggle"), ESearchCase::IgnoreCase)
		|| Verb.Equals(TEXT("lighting"), ESearchCase::IgnoreCase)
		|| Verb.Equals(TEXT("pwrhelp"), ESearchCase::IgnoreCase);
}

static bool ShouldAnnounceLoadToPlayer(AFGPlayerController* PlayerController, UWorld* World)
{
	if (!IsValid(PlayerController) || !IsValid(World))
	{
		return false;
	}

	const ENetMode NetMode = World->GetNetMode();

	// Dedicated/listen: BegunPlay runs on the joining client — announce locally there.
	if (NetMode == NM_Client)
	{
		return PlayerController->IsLocalController();
	}

	// Dedicated server process: skip headless local PC.
	if (NetMode == NM_DedicatedServer && PlayerController->IsLocalController())
	{
		return false;
	}

	return PlayerController->GetNetConnection() != nullptr || NetMode == NM_Standalone;
}

static void AnnounceModLoadedForPlayer(AFGPlayerController* PlayerController)
{
	if (!IsValid(PlayerController))
	{
		return;
	}

	for (auto It = PlayersWithLoadAnnouncement.CreateIterator(); It; ++It)
	{
		if (!It->IsValid())
		{
			It.RemoveCurrent();
		}
	}

	if (PlayersWithLoadAnnouncement.Contains(PlayerController))
	{
		return;
	}

	PlayersWithLoadAnnouncement.Add(PlayerController);
	SendSystemChat(PlayerController, TEXT("Structural Power loaded."));
}

static void ScheduleLoadAnnouncement(AFGPlayerController* PlayerController)
{
	if (!IsValid(PlayerController))
	{
		return;
	}

	UWorld* World = PlayerController->GetWorld();
	if (!ShouldAnnounceLoadToPlayer(PlayerController, World))
	{
		return;
	}

	// BegunPlay fires before the client can reliably receive Client_SendChatMessage on dedicated.
	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(
		Handle,
		[PlayerControllerWeak = TWeakObjectPtr<AFGPlayerController>(PlayerController)]()
		{
			AnnounceModLoadedForPlayer(PlayerControllerWeak.Get());
		},
		2.0f,
		false);
}

static bool TryToggleTrace(UWorld* World, FString& OutMessage)
{
	const bool bOn = !FStructuralPowerModConfig::IsTraceEnabled();
	const TArray<FString> Args = {TEXT("Trace"), bOn ? TEXT("1") : TEXT("0")};
	if (!FStructuralPowerModConfig::TryApplySetCommand(Args, World))
	{
		return false;
	}

	OutMessage = bOn
		? TEXT("PWR trace logging enabled.")
		: TEXT("PWR trace logging disabled.");
	return true;
}

static bool TryToggleGroupLighting(UWorld* World, FString& OutMessage)
{
	const bool bOn = !FStructuralPowerModConfig::IsGroupLightingEnabled();
	const TArray<FString> Args = {TEXT("GroupLighting"), bOn ? TEXT("1") : TEXT("0")};
	if (!FStructuralPowerModConfig::TryApplySetCommand(Args, World))
	{
		return false;
	}

	if (AStructuralPowerGraphSubsystem* Graph = AStructuralPowerGraphSubsystem::Find(World))
	{
		Graph->ReconcileAllLightConsumers();
	}

	OutMessage = bOn
		? TEXT("Structural lighting enabled — unwired lights on powered foundations may draw.")
		: TEXT("Structural lighting disabled — lights need vanilla wires.");
	return true;
}

static bool TrySetHoverMultiplier(
	UWorld* World,
	const TCHAR* ConfigKey,
	const TCHAR* AxisLabel,
	float Value,
	FString& OutMessage)
{
	const TArray<FString> Args = {ConfigKey, FString::SanitizeFloat(Value)};
	if (!FStructuralPowerModConfig::TryApplySetCommand(Args, World))
	{
		return false;
	}

	OutMessage = FString::Printf(
		TEXT("Hoverpack %s reach multiplier set to %s."),
		AxisLabel,
		*Args[1]);
	return true;
}

static void SendHelp(AFGPlayerController* PlayerController)
{
	SendSystemChat(
		PlayerController,
		TEXT("!HoverH <1-10>  !HoverV <1-10>  — hoverpack reach multipliers"),
		FLinearColor::Yellow);
	SendSystemChat(
		PlayerController,
		TEXT("!tracetoggle  — debug: flip verbose [HALSP] logging in FactoryGame.log"),
		FLinearColor::Yellow);
	SendSystemChat(
		PlayerController,
		TEXT("!lighting  — toggle structural lighting (M3; default off)"),
		FLinearColor::Yellow);
}
}

void FStructuralPowerBangCommands::RegisterChatHook()
{
	if (BangChatHookHandle.IsValid())
	{
		return;
	}

	BangChatHookHandle = AFGPlayerController::PlayerControllerBegunPlay.AddLambda(
		[](AFGPlayerController* PlayerController)
		{
			if (!IsValid(PlayerController))
			{
				return;
			}

			ScheduleLoadAnnouncement(PlayerController);

			PlayerController->ChatMessageEntered.AddLambda(
				[PlayerControllerWeak = TWeakObjectPtr<AFGPlayerController>(PlayerController)](
					const FString& ChatMessage,
					bool& bShouldSendMessage)
				{
					const FString Trimmed = ChatMessage.TrimStartAndEnd();
					if (!Trimmed.StartsWith(TEXT("!")))
					{
						return;
					}

					const FString CommandLine = Trimmed.Mid(1);
					if (!IsStructuralPowerCommand(CommandLine))
					{
						return;
					}

					AFGPlayerController* PlayerController = PlayerControllerWeak.Get();
					if (!IsValid(PlayerController))
					{
						return;
					}

					if (UStructuralPowerRCO* Rco = PlayerController->GetRemoteCallObjectOfClass<UStructuralPowerRCO>())
					{
						Rco->Server_RunBangCommand(CommandLine);
						bShouldSendMessage = false;
					}
				});
		});
}

void FStructuralPowerBangCommands::Execute(AFGPlayerController* PlayerController, const FString& CommandLine)
{
	if (!IsValid(PlayerController))
	{
		return;
	}

	UWorld* World = PlayerController->GetWorld();
	if (!IsValid(World) || World->GetNetMode() == NM_Client)
	{
		SendSystemChat(PlayerController, TEXT("Structural Power chat commands run on the server only."), FLinearColor::Red);
		return;
	}

	TArray<FString> Tokens;
	CommandLine.ParseIntoArrayWS(Tokens);
	if (Tokens.Num() == 0)
	{
		SendHelp(PlayerController);
		return;
	}

	const FString Verb = Tokens[0];
	FString Feedback;

	if (Verb.Equals(TEXT("pwrhelp"), ESearchCase::IgnoreCase))
	{
		SendHelp(PlayerController);
		return;
	}

	if (Verb.Equals(TEXT("HoverH"), ESearchCase::IgnoreCase))
	{
		if (Tokens.Num() < 2)
		{
			SendSystemChat(PlayerController, TEXT("Use !HoverH <1-10>"), FLinearColor::Red);
			return;
		}

		if (TrySetHoverMultiplier(
			World,
			TEXT("HoverpackStructuralHorizontalMultiplier"),
			TEXT("horizontal"),
			FCString::Atof(*Tokens[1]),
			Feedback))
		{
			SendSystemChat(PlayerController, Feedback);
		}
		return;
	}

	if (Verb.Equals(TEXT("HoverV"), ESearchCase::IgnoreCase))
	{
		if (Tokens.Num() < 2)
		{
			SendSystemChat(PlayerController, TEXT("Use !HoverV <1-10>"), FLinearColor::Red);
			return;
		}

		if (TrySetHoverMultiplier(
			World,
			TEXT("HoverpackStructuralVerticalMultiplier"),
			TEXT("vertical"),
			FCString::Atof(*Tokens[1]),
			Feedback))
		{
			SendSystemChat(PlayerController, Feedback);
		}
		return;
	}

	if (Verb.Equals(TEXT("tracetoggle"), ESearchCase::IgnoreCase))
	{
		if (TryToggleTrace(World, Feedback))
		{
			SendSystemChat(PlayerController, Feedback);
		}
		return;
	}

	if (Verb.Equals(TEXT("lighting"), ESearchCase::IgnoreCase))
	{
		if (TryToggleGroupLighting(World, Feedback))
		{
			SendSystemChat(PlayerController, Feedback);
		}
		return;
	}

	SendSystemChat(
		PlayerController,
		FString::Printf(TEXT("Unknown Structural Power command: !%s (try !pwrhelp)"), *Verb),
		FLinearColor::Red);
}
