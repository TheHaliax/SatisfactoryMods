// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Network/UPCChatRCO.h"

#include "Command/FPCBangCommands.h"
#include "FGPlayerController.h"
#include "Net/UnrealNetwork.h"
#include "PipelineColorLog.h"
#include "Store/APCSwatchStoreSubsystem.h"
#include "Store/FPCSwatchSlotDispatch.h"

void UPCChatRCO::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UPCChatRCO, mForceNetField_UPCChatRCO);
}

void UPCChatRCO::Server_RunBangCommand_Implementation(const FString& CommandLine)
{
	if (AFGPlayerController* PlayerController = GetOwnerPlayerController())
	{
		FPCBangCommands::Execute(PlayerController, CommandLine);
	}
}

void UPCChatRCO::Server_SetActivePcSwatch_Implementation(
	TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Swatch)
{
	FPCSwatchSlotDispatch::SetActivePcDesc(GetOwnerPlayerController(), Swatch);
}

void UPCChatRCO::Server_SetPcSwatchColors_Implementation(
	TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Swatch,
	FFactoryCustomizationColorSlot ColorData)
{
	if (!APCSwatchStoreSubsystem::IsPCCustomization(Swatch))
	{
		return;
	}

	UWorld* World = GetWorld();
	APCSwatchStoreSubsystem* Store = APCSwatchStoreSubsystem::GetOrCreate(World);
	if (!Store || !Store->HasAuthority())
	{
		return;
	}

	const FName Key = APCSwatchStoreSubsystem::KeyFromSwatchStatic(Swatch);
	if (Key.IsNone())
	{
		return;
	}

	FPCSwatchSlotDispatch::SetActivePcDesc(GetOwnerPlayerController(), Swatch);
	Store->SetFromSlot(Key, ColorData);
	UE_LOG(LogPipelineColor, Log, TEXT("%s RCO SetPcSwatchColors key=%s"),
		PIPELINECOLOR_LOG_PREFIX, *Key.ToString());
}
