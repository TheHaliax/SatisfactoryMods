// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Command/FPCBangCommands.h"

#include "Config/FPCPipelineColorModConfig.h"
#include "FGChatManager.h"
#include "FGGameState.h"
#include "FGPlayerController.h"
#include "Network/UPCChatRCO.h"
#include "Swatches/UPCSwatchDescs.h"

namespace
{
FDelegateHandle BangChatHookHandle;

constexpr const TCHAR* BangSenderName = TEXT("Hal");

void SendBangChat(
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
	MessageStruct.MessageSender = FText::FromString(BangSenderName);

	const bool bUseLocalChatBuffer = World->GetNetMode() == NM_Standalone
		|| (World->GetNetMode() == NM_Client && PlayerController->IsLocalController());

	if (bUseLocalChatBuffer)
	{
		if (AFGChatManager* ChatManager = AFGChatManager::Get(World))
		{
			ChatManager->AddChatMessageToReceived(MessageStruct);
		}
	}
	else
	{
		PlayerController->Client_SendChatMessage(MessageStruct);
	}
}

bool IsPipelineColorCommand(const FString& CommandLine)
{
	TArray<FString> Tokens;
	CommandLine.ParseIntoArrayWS(Tokens);
	if (Tokens.Num() == 0)
	{
		return false;
	}

	const FString& Verb = Tokens[0];
	return Verb.Equals(TEXT("Metallic"), ESearchCase::IgnoreCase)
		|| Verb.Equals(TEXT("pchelp"), ESearchCase::IgnoreCase);
}

FString NormalizeFluidQuery(const FString& In)
{
	FString Lower = In.ToLower().TrimStartAndEnd();
	if (Lower.StartsWith(TEXT("pc ")))
	{
		Lower = Lower.Mid(3).TrimStartAndEnd();
	}

	FString Out;
	Out.Reserve(Lower.Len());
	for (const TCHAR C : Lower)
	{
		if (FChar::IsAlnum(C))
		{
			Out.AppendChar(C);
		}
	}
	return Out;
}

FString FriendlyNameFromKey(FName Key)
{
	const FString Raw = Key.ToString();
	FString Out;
	Out.Reserve(Raw.Len() + 8);
	for (int32 i = 0; i < Raw.Len(); ++i)
	{
		const TCHAR C = Raw[i];
		if (i > 0 && FChar::IsUpper(C) && FChar::IsLower(Raw[i - 1]))
		{
			Out.AppendChar(TEXT(' '));
		}
		Out.AppendChar(C);
	}
	return Out;
}

struct FFluidAlias
{
	FName Key;
	FString Display;
};

void CollectFluidAliases(TArray<FFluidAlias>& Out)
{
	Out.Reset();
	const TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Descs[] = {
		UPCSwatchDesc_Neutral::StaticClass(),
		UPCSwatchDesc_Water::StaticClass(),
		UPCSwatchDesc_CrudeOil::StaticClass(),
		UPCSwatchDesc_HeavyOilResidue::StaticClass(),
		UPCSwatchDesc_Fuel::StaticClass(),
		UPCSwatchDesc_Turbofuel::StaticClass(),
		UPCSwatchDesc_LiquidBiofuel::StaticClass(),
		UPCSwatchDesc_AluminaSolution::StaticClass(),
		UPCSwatchDesc_SulfuricAcid::StaticClass(),
		UPCSwatchDesc_DissolvedSilica::StaticClass(),
		UPCSwatchDesc_NitricAcid::StaticClass(),
		UPCSwatchDesc_DarkMatterResidue::StaticClass(),
		UPCSwatchDesc_ExcitedPhotonicMatter::StaticClass(),
		UPCSwatchDesc_IonizedFuel::StaticClass(),
		UPCSwatchDesc_RocketFuel::StaticClass(),
		UPCSwatchDesc_NitrogenGas::StaticClass(),
		UPCSwatchDesc_Fallback::StaticClass(),
	};

	for (const TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Desc : Descs)
	{
		const UPCSwatchDescBase* CDO = Cast<UPCSwatchDescBase>(Desc->GetDefaultObject());
		if (!CDO || CDO->CatalogKey.IsNone())
		{
			continue;
		}
		FFluidAlias Alias;
		Alias.Key = CDO->CatalogKey;
		Alias.Display = FString::Printf(TEXT("PC %s"), *FriendlyNameFromKey(Alias.Key));
		Out.Add(Alias);
	}
}

bool ResolveFluidQuery(const FString& Query, FName& OutKey, FString& OutFriendly)
{
	const FString Norm = NormalizeFluidQuery(Query);
	if (Norm.IsEmpty())
	{
		return false;
	}

	TArray<FFluidAlias> Aliases;
	CollectFluidAliases(Aliases);

	for (const FFluidAlias& Alias : Aliases)
	{
		if (NormalizeFluidQuery(Alias.Key.ToString()) == Norm
			|| NormalizeFluidQuery(Alias.Display) == Norm
			|| NormalizeFluidQuery(FriendlyNameFromKey(Alias.Key)) == Norm)
		{
			OutKey = Alias.Key;
			OutFriendly = FriendlyNameFromKey(Alias.Key);
			return true;
		}
	}
	return false;
}

void SendHelp(AFGPlayerController* PlayerController)
{
	SendBangChat(
		PlayerController,
		TEXT("!Metallic <fluid>  — toggle metallic finish for that fluid"),
		FLinearColor::Yellow);
	SendBangChat(
		PlayerController,
		TEXT("!pchelp  — list Pipeline Color chat commands"),
		FLinearColor::Yellow);
}

bool TryToggleMetallic(UWorld* World, FName Key, FString& OutMessage)
{
	const bool bNext = !FPCPipelineColorModConfig::IsMetallicForKey(Key);
	if (!FPCPipelineColorModConfig::TrySetMetallicOverride(Key, bNext, World))
	{
		OutMessage = TEXT("Cannot change metallic on client.");
		return false;
	}

	OutMessage = FString::Printf(
		TEXT("%s metallic %s."),
		*FriendlyNameFromKey(Key),
		bNext ? TEXT("on") : TEXT("off"));
	return true;
}
} // namespace

void FPCBangCommands::RegisterChatHook()
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
					if (!IsPipelineColorCommand(CommandLine))
					{
						return;
					}

					AFGPlayerController* PC = PlayerControllerWeak.Get();
					if (!IsValid(PC))
					{
						return;
					}

					if (UPCChatRCO* Rco = PC->GetRemoteCallObjectOfClass<UPCChatRCO>())
					{
						Rco->Server_RunBangCommand(CommandLine);
						bShouldSendMessage = false;
					}
				});
		});
}

void FPCBangCommands::Execute(AFGPlayerController* PlayerController, const FString& CommandLine)
{
	if (!IsValid(PlayerController))
	{
		return;
	}

	UWorld* World = PlayerController->GetWorld();
	if (!IsValid(World) || World->GetNetMode() == NM_Client)
	{
		SendBangChat(
			PlayerController,
			TEXT("Pipeline Color chat commands run on the server only."),
			FLinearColor::Red);
		return;
	}

	const FString Trimmed = CommandLine.TrimStartAndEnd();
	int32 SpaceIdx = INDEX_NONE;
	Trimmed.FindChar(TEXT(' '), SpaceIdx);
	const FString Verb = SpaceIdx == INDEX_NONE ? Trimmed : Trimmed.Left(SpaceIdx);
	const FString Rest = SpaceIdx == INDEX_NONE
		? FString()
		: Trimmed.Mid(SpaceIdx + 1).TrimStartAndEnd();

	if (Verb.Equals(TEXT("pchelp"), ESearchCase::IgnoreCase))
	{
		SendHelp(PlayerController);
		return;
	}

	if (Verb.Equals(TEXT("Metallic"), ESearchCase::IgnoreCase))
	{
		if (Rest.IsEmpty())
		{
			SendBangChat(PlayerController, TEXT("Use !Metallic <fluid>"), FLinearColor::Red);
			return;
		}

		FName Key;
		FString Friendly;
		if (!ResolveFluidQuery(Rest, Key, Friendly))
		{
			SendBangChat(
				PlayerController,
				FString::Printf(TEXT("Unknown fluid: %s"), *Rest),
				FLinearColor::Red);
			return;
		}

		FString Feedback;
		if (TryToggleMetallic(World, Key, Feedback))
		{
			SendBangChat(PlayerController, Feedback);
		}
		else
		{
			SendBangChat(PlayerController, Feedback, FLinearColor::Red);
		}
		return;
	}

	SendBangChat(
		PlayerController,
		FString::Printf(TEXT("Unknown Pipeline Color command: !%s (try !pchelp)"), *Verb),
		FLinearColor::Red);
}
