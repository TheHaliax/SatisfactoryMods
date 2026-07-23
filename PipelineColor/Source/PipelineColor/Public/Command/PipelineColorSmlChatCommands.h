// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "Command/ChatCommandInstance.h"
#include "CoreMinimal.h"
#include "PipelineColorSmlChatCommands.generated.h"

class UWorld;

class PIPELINECOLOR_API FPipelineColorSmlChatCommands {
 public:
  static void RegisterWithWorld(UWorld* World);
};

UCLASS(NotPlaceable)
class APCMetallicChatCommand : public AChatCommandInstance {
  GENERATED_BODY()

 public:
  APCMetallicChatCommand();

  virtual EExecutionStatus ExecuteCommand_Implementation(UCommandSender* Sender,
                                                         const TArray<FString>& Arguments,
                                                         const FString& Label) override;
};

UCLASS(NotPlaceable)
class APCPcChatCommand : public AChatCommandInstance {
  GENERATED_BODY()

 public:
  APCPcChatCommand();

  virtual EExecutionStatus ExecuteCommand_Implementation(UCommandSender* Sender,
                                                         const TArray<FString>& Arguments,
                                                         const FString& Label) override;
};

UCLASS(NotPlaceable)
class APCPchelpChatCommand : public AChatCommandInstance {
  GENERATED_BODY()

 public:
  APCPchelpChatCommand();

  virtual EExecutionStatus ExecuteCommand_Implementation(UCommandSender* Sender,
                                                         const TArray<FString>& Arguments,
                                                         const FString& Label) override;
};
