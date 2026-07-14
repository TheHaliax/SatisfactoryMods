// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Command/ChatCommandInstance.h"
#include "StructuralPowerSmlChatCommands.generated.h"

class UWorld;

class STRUCTURALPOWER_API FStructuralPowerSmlChatCommands
{
public:
	static void RegisterWithWorld(UWorld* World);
};

UCLASS(NotPlaceable)
class AStructuralPowerLightingChatCommand : public AChatCommandInstance
{
	GENERATED_BODY()

public:
	AStructuralPowerLightingChatCommand();

	virtual EExecutionStatus ExecuteCommand_Implementation(
		UCommandSender* Sender,
		const TArray<FString>& Arguments,
		const FString& Label) override;
};

UCLASS(NotPlaceable)
class AStructuralPowerHoverHChatCommand : public AChatCommandInstance
{
	GENERATED_BODY()

public:
	AStructuralPowerHoverHChatCommand();

	virtual EExecutionStatus ExecuteCommand_Implementation(
		UCommandSender* Sender,
		const TArray<FString>& Arguments,
		const FString& Label) override;
};

UCLASS(NotPlaceable)
class AStructuralPowerHoverVChatCommand : public AChatCommandInstance
{
	GENERATED_BODY()

public:
	AStructuralPowerHoverVChatCommand();

	virtual EExecutionStatus ExecuteCommand_Implementation(
		UCommandSender* Sender,
		const TArray<FString>& Arguments,
		const FString& Label) override;
};

UCLASS(NotPlaceable)
class AStructuralPowerPwrHelpChatCommand : public AChatCommandInstance
{
	GENERATED_BODY()

public:
	AStructuralPowerPwrHelpChatCommand();

	virtual EExecutionStatus ExecuteCommand_Implementation(
		UCommandSender* Sender,
		const TArray<FString>& Arguments,
		const FString& Label) override;
};
