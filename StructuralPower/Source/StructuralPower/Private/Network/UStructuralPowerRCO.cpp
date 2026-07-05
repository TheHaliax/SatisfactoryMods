// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Network/UStructuralPowerRCO.h"

#include "Buildables/FGBuildable.h"
#include "Command/FStructuralPowerBangCommands.h"
#include "Equipment/FStructuralHoverpackBridge.h"
#include "FGPlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Save/AStructuralPowerGraphSubsystem.h"
#include "StructuralPowerLog.h"

void UStructuralPowerRCO::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UStructuralPowerRCO, mForceNetField_UStructuralPowerRCO);
}

void UStructuralPowerRCO::Server_SetPlayerOverrideId_Implementation(
	AFGBuildable* Buildable,
	FName PlayerOverrideId)
{
	if (!IsValid(Buildable) || !Buildable->HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World) || World->GetNetMode() == NM_Client)
	{
		return;
	}

	AStructuralPowerGraphSubsystem* Graph = AStructuralPowerGraphSubsystem::Find(World);
	if (!Graph)
	{
		UE_LOG(LogStructuralPower, Warning, TEXT("[PWR] RCO SetPlayerOverrideId — no graph subsystem"));
		return;
	}

	Graph->SetPlayerOverrideId(Buildable, PlayerOverrideId);

	UE_LOG(LogStructuralPower, Log,
		TEXT("[PWR] RCO SetPlayerOverrideId %s id=%s"),
		*Buildable->GetName(),
		PlayerOverrideId.IsNone() ? TEXT("(inherit)") : *PlayerOverrideId.ToString());
}

void UStructuralPowerRCO::Server_RunBangCommand_Implementation(const FString& CommandLine)
{
	if (AFGPlayerController* PlayerController = GetOwnerPlayerController())
	{
		FStructuralPowerBangCommands::Execute(PlayerController, CommandLine);
	}
}

void UStructuralPowerRCO::Client_SyncHoverpackTether_Implementation(
	const FVector& Anchor,
	float MaxHorizontal,
	float MaxVertical)
{
	FStructuralHoverpackBridge::ApplyClientTetherMirror(Anchor, MaxHorizontal, MaxVertical);
}

void UStructuralPowerRCO::Client_ClearHoverpackTether_Implementation()
{
	FStructuralHoverpackBridge::ClearClientTetherMirror();
}
