// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Command/FPCBangCommands.h"

#include "Config/FPCPipelineColorModConfig.h"
#include "FGChatManager.h"
#include "FGGameState.h"
#include "FGPlayerController.h"
#include "Network/UPCChatRCO.h"
#include "PipelineColorRootInstanceModule.h"
#include "Store/APCSwatchStoreSubsystem.h"
#include "Swatches/FPCDynamicSwatchRegistry.h"
#include "Swatches/UPCSwatchDescs.h"

namespace {
FDelegateHandle BangChatHookHandle;

constexpr const TCHAR* BangSenderName = TEXT("Hal");

void SendBangChat(AFGPlayerController* PlayerController, const FString& Message,
                  const FLinearColor& Color = FLinearColor::Green) {
  if (!IsValid(PlayerController)) {
    return;
  }

  UWorld* World = PlayerController->GetWorld();
  if (!IsValid(World)) {
    return;
  }

  AGameStateBase* GameState = World->GetGameState();
  if (!GameState) {
    return;
  }

  FChatMessageStruct MessageStruct;
  MessageStruct.MessageText = FText::FromString(Message);
  MessageStruct.MessageType = EFGChatMessageType::CMT_CustomMessage;
  MessageStruct.ServerTimeStamp = GameState->GetServerWorldTimeSeconds();
  MessageStruct.MessageSenderColor = Color;
  MessageStruct.MessageSender = FText::FromString(BangSenderName);

  const bool bUseLocalChatBuffer =
      World->GetNetMode() == NM_Standalone ||
      (World->GetNetMode() == NM_Client && PlayerController->IsLocalController());

  if (bUseLocalChatBuffer) {
    if (AFGChatManager* ChatManager = AFGChatManager::Get(World)) {
      ChatManager->AddChatMessageToReceived(MessageStruct);
    }
  } else {
    PlayerController->Client_SendChatMessage(MessageStruct);
  }
}

bool IsPipelineColorCommand(const FString& CommandLine) {
  TArray<FString> Tokens;
  CommandLine.ParseIntoArrayWS(Tokens);
  if (Tokens.Num() == 0) {
    return false;
  }

  const FString& Verb = Tokens[0];
  return Verb.Equals(TEXT("Metallic"), ESearchCase::IgnoreCase) ||
         Verb.Equals(TEXT("pc"), ESearchCase::IgnoreCase) ||
         Verb.Equals(TEXT("pchelp"), ESearchCase::IgnoreCase);
}

FString NormalizeFluidQuery(const FString& In) {
  FString Lower = In.ToLower().TrimStartAndEnd();
  if (Lower.StartsWith(TEXT("pc "))) {
    Lower = Lower.Mid(3).TrimStartAndEnd();
  }

  FString Out;
  Out.Reserve(Lower.Len());
  for (const TCHAR C : Lower) {
    if (FChar::IsAlnum(C)) {
      Out.AppendChar(C);
    }
  }
  return Out;
}

FString FriendlyNameFromKey(FName Key) {
  const FString Raw = Key.ToString();
  FString Out;
  Out.Reserve(Raw.Len() + 8);
  for (int32 i = 0; i < Raw.Len(); ++i) {
    const TCHAR C = Raw[i];
    if (i > 0 && FChar::IsUpper(C) && FChar::IsLower(Raw[i - 1])) {
      Out.AppendChar(TEXT(' '));
    }
    Out.AppendChar(C);
  }
  return Out;
}

struct FFluidAlias {
  FName Key;
  FString Display;
};

void CollectFluidAliases(TArray<FFluidAlias>& Out) {
  Out.Reset();
  TArray<TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch>> Descs;
  FPCDynamicSwatchRegistry::AppendSwatchClasses(Descs);

  for (const TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch>& Desc : Descs) {
    if (!Desc) {
      continue;
    }
    const UPCSwatchDescBase* CDO = Cast<UPCSwatchDescBase>(Desc->GetDefaultObject());
    if (!CDO || CDO->CatalogKey.IsNone()) {
      continue;
    }
    FFluidAlias Alias;
    Alias.Key = CDO->CatalogKey;
    Alias.Display = CDO->mDisplayName.ToString();
    if (Alias.Display.IsEmpty()) {
      Alias.Display = FriendlyNameFromKey(Alias.Key);
    }
    Out.Add(Alias);
  }
}

bool ResolveFluidQuery(const FString& Query, FName& OutKey, FString& OutFriendly) {
  const FString Norm = NormalizeFluidQuery(Query);
  if (Norm.IsEmpty()) {
    return false;
  }

  TArray<FFluidAlias> Aliases;
  CollectFluidAliases(Aliases);

  for (const FFluidAlias& Alias : Aliases) {
    if (NormalizeFluidQuery(Alias.Key.ToString()) == Norm ||
        NormalizeFluidQuery(Alias.Display) == Norm ||
        NormalizeFluidQuery(FriendlyNameFromKey(Alias.Key)) == Norm) {
      OutKey = Alias.Key;
      OutFriendly = FriendlyNameFromKey(Alias.Key);
      return true;
    }
  }
  return false;
}

void SendHelp(AFGPlayerController* PlayerController) {
  SendBangChat(PlayerController, TEXT("!Metallic <fluid>  — toggle metallic for that fluid"),
               FLinearColor::Yellow);
  SendBangChat(PlayerController, TEXT("!Metallic all on  — force every fluid metallic on"),
               FLinearColor::Yellow);
  SendBangChat(PlayerController,
               TEXT("!Metallic all off  — force every fluid metallic off (color)"),
               FLinearColor::Yellow);
  SendBangChat(PlayerController,
               TEXT("!Metallic default  — clear metallic overrides; gas on / liquid off"),
               FLinearColor::Yellow);
  SendBangChat(PlayerController, TEXT("!pc <fluid> liquid|gas  — pick mFluidColor / mGasColor"),
               FLinearColor::Yellow);
  SendBangChat(PlayerController,
               TEXT("!pc default  — clear custom swatch colors (catalog defaults)"),
               FLinearColor::Yellow);
  SendBangChat(PlayerController, TEXT("!pchelp  — list Pipeline Color chat commands"),
               FLinearColor::Yellow);
}

bool TryParseColorSourceToken(const FString& Token, EPCColorSource& Out) {
  if (Token.Equals(TEXT("gas"), ESearchCase::IgnoreCase)) {
    Out = EPCColorSource::Gas;
    return true;
  }
  if (Token.Equals(TEXT("liquid"), ESearchCase::IgnoreCase) ||
      Token.Equals(TEXT("fluid"), ESearchCase::IgnoreCase)) {
    Out = EPCColorSource::Liquid;
    return true;
  }
  return false;
}

bool TryParsePcColorSource(const FString& Rest, FString& OutFluidQuery, EPCColorSource& OutSource) {
  TArray<FString> Tokens;
  Rest.ParseIntoArrayWS(Tokens);
  if (Tokens.Num() < 2) {
    return false;
  }
  if (!TryParseColorSourceToken(Tokens.Last(), OutSource)) {
    return false;
  }
  Tokens.RemoveAt(Tokens.Num() - 1);
  OutFluidQuery = FString::Join(Tokens, TEXT(" "));
  return !OutFluidQuery.IsEmpty();
}

bool TryToggleMetallic(UWorld* World, FName Key, FString& OutMessage) {
  const bool bNext = !FPCPipelineColorModConfig::IsMetallicForKey(Key);
  if (!FPCPipelineColorModConfig::TrySetMetallicOverride(Key, bNext, World)) {
    OutMessage = TEXT("Host only.");
    return false;
  }

  OutMessage = FString::Printf(TEXT("%s metallic %s."), *FriendlyNameFromKey(Key),
                               bNext ? TEXT("on") : TEXT("off"));
  return true;
}

bool TryParseAllMetallic(const FString& Rest, bool& bOutOn) {
  TArray<FString> Tokens;
  Rest.ParseIntoArrayWS(Tokens);
  if (Tokens.Num() < 2 || !Tokens[0].Equals(TEXT("all"), ESearchCase::IgnoreCase)) {
    return false;
  }
  if (Tokens[1].Equals(TEXT("on"), ESearchCase::IgnoreCase) ||
      Tokens[1].Equals(TEXT("1"), ESearchCase::IgnoreCase) ||
      Tokens[1].Equals(TEXT("true"), ESearchCase::IgnoreCase)) {
    bOutOn = true;
    return true;
  }
  if (Tokens[1].Equals(TEXT("off"), ESearchCase::IgnoreCase) ||
      Tokens[1].Equals(TEXT("0"), ESearchCase::IgnoreCase) ||
      Tokens[1].Equals(TEXT("false"), ESearchCase::IgnoreCase)) {
    bOutOn = false;
    return true;
  }
  return false;
}
} // namespace

void FPCBangCommands::RegisterChatHook() {
  if (BangChatHookHandle.IsValid()) {
    return;
  }

  BangChatHookHandle = AFGPlayerController::PlayerControllerBegunPlay.AddLambda(
      [](AFGPlayerController* PlayerController) {
        if (!IsValid(PlayerController)) {
          return;
        }

        PlayerController->ChatMessageEntered.AddLambda(
            [PlayerControllerWeak = TWeakObjectPtr<AFGPlayerController>(PlayerController)](
                const FString& ChatMessage, bool& bShouldSendMessage) {
              const FString Trimmed = ChatMessage.TrimStartAndEnd();
              if (!Trimmed.StartsWith(TEXT("!"))) {
                return;
              }

              const FString CommandLine = Trimmed.Mid(1);
              if (!IsPipelineColorCommand(CommandLine)) {
                return;
              }

              AFGPlayerController* PC = PlayerControllerWeak.Get();
              if (!IsValid(PC)) {
                return;
              }

              if (UPCChatRCO* Rco = PC->GetRemoteCallObjectOfClass<UPCChatRCO>()) {
                Rco->Server_RunBangCommand(CommandLine);
                bShouldSendMessage = false;
              }
            });
      });
}

void FPCBangCommands::Execute(AFGPlayerController* PlayerController, const FString& CommandLine) {
  if (!IsValid(PlayerController)) {
    return;
  }

  UWorld* World = PlayerController->GetWorld();
  if (!IsValid(World) || World->GetNetMode() == NM_Client) {
    SendBangChat(PlayerController, TEXT("Host only."), FLinearColor::Red);
    return;
  }

  FPCDynamicSwatchRegistry::Ensure(World, UPipelineColorRootInstanceModule::Find(World));

  const FString Trimmed = CommandLine.TrimStartAndEnd();
  int32 SpaceIdx = INDEX_NONE;
  Trimmed.FindChar(TEXT(' '), SpaceIdx);
  const FString Verb = SpaceIdx == INDEX_NONE ? Trimmed : Trimmed.Left(SpaceIdx);
  const FString Rest =
      SpaceIdx == INDEX_NONE ? FString() : Trimmed.Mid(SpaceIdx + 1).TrimStartAndEnd();

  if (Verb.Equals(TEXT("pchelp"), ESearchCase::IgnoreCase)) {
    SendHelp(PlayerController);
    return;
  }

  if (Verb.Equals(TEXT("pc"), ESearchCase::IgnoreCase)) {
    if (Rest.Equals(TEXT("default"), ESearchCase::IgnoreCase)) {
      APCSwatchStoreSubsystem* Store = APCSwatchStoreSubsystem::GetOrCreate(World);
      if (!Store) {
        SendBangChat(PlayerController, TEXT("No store."), FLinearColor::Red);
        return;
      }

      Store->ReseedAllFromCatalog();
      SendBangChat(PlayerController, TEXT("Custom swatches cleared."));
      return;
    }

    FString FluidQuery;
    EPCColorSource Source = EPCColorSource::Liquid;
    if (!TryParsePcColorSource(Rest, FluidQuery, Source)) {
      SendBangChat(PlayerController, TEXT("!pc <fluid> liquid|gas | default"), FLinearColor::Red);
      return;
    }

    FName Key;
    FString Friendly;
    if (!ResolveFluidQuery(FluidQuery, Key, Friendly)) {
      SendBangChat(PlayerController, FString::Printf(TEXT("Unknown: %s"), *FluidQuery),
                   FLinearColor::Red);
      return;
    }

    if (!FPCPipelineColorModConfig::TrySetColorSource(Key, Source, World)) {
      SendBangChat(PlayerController, TEXT("Host only."), FLinearColor::Red);
      return;
    }

    if (APCSwatchStoreSubsystem* Store = APCSwatchStoreSubsystem::GetOrCreate(World)) {
      Store->ReseedKeyColorsFromCatalog(Key);
    }

    const TCHAR* SourceLabel = Source == EPCColorSource::Gas ? TEXT("gas") : TEXT("liquid");
    SendBangChat(PlayerController, FString::Printf(TEXT("%s %s color."), *Friendly, SourceLabel));
    return;
  }

  if (Verb.Equals(TEXT("Metallic"), ESearchCase::IgnoreCase)) {
    if (Rest.IsEmpty()) {
      SendBangChat(PlayerController, TEXT("!Metallic <fluid|all on|all off|default>"),
                   FLinearColor::Red);
      return;
    }

    if (Rest.Equals(TEXT("default"), ESearchCase::IgnoreCase)) {
      if (!FPCPipelineColorModConfig::TryResetMetallicToDefaults(World)) {
        SendBangChat(PlayerController, TEXT("Host only."), FLinearColor::Red);
        return;
      }

      SendBangChat(PlayerController, TEXT("Metallic defaults restored."));
      return;
    }

    bool bAllOn = false;
    if (TryParseAllMetallic(Rest, bAllOn)) {
      if (!FPCPipelineColorModConfig::TrySetAllMetallic(bAllOn, World)) {
        SendBangChat(PlayerController, TEXT("Host only."), FLinearColor::Red);
        return;
      }
      SendBangChat(PlayerController, bAllOn ? TEXT("All metallic on.") : TEXT("All metallic off."));
      return;
    }

    FName Key;
    FString Friendly;
    if (!ResolveFluidQuery(Rest, Key, Friendly)) {
      SendBangChat(PlayerController, FString::Printf(TEXT("Unknown: %s"), *Rest),
                   FLinearColor::Red);
      return;
    }

    FString Feedback;
    if (TryToggleMetallic(World, Key, Feedback)) {
      SendBangChat(PlayerController, Feedback);
    } else {
      SendBangChat(PlayerController, Feedback, FLinearColor::Red);
    }
    return;
  }

  SendBangChat(PlayerController, TEXT("Unknown. !pchelp"), FLinearColor::Red);
}
