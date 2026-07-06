// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Network/UStructuralPowerRCO.h"

#include "Buildables/FGBuildable.h"
#include "Command/FStructuralPowerBangCommands.h"
#include "Equipment/FStructuralHoverpackBridge.h"
#include "FGPlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Rules/FStructuralEligibilityRules.h"
#include "Save/AStructuralPowerGraphSubsystem.h"
#include "StructuralPowerLog.h"
#include "UI/FStructuralPowerIdPresenterFactory.h"

void UStructuralPowerRCO::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UStructuralPowerRCO, mForceNetField_UStructuralPowerRCO);
}

static AStructuralPowerGraphSubsystem* GetAuthorityGraph(UWorld* World)
{
	if (!IsValid(World) || World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}

	return AStructuralPowerGraphSubsystem::Find(World);
}

void UStructuralPowerRCO::Server_SetEndpointIds_Implementation(
	AFGBuildable* Buildable,
	FName Source,
	FName Control,
	bool bClearSource,
	bool bClearControl)
{
	if (!IsValid(Buildable) || !Buildable->HasAuthority())
	{
		return;
	}

	if (!FStructuralEligibilityRules::IsIdConfigTarget(Buildable))
	{
		UE_LOG(LogStructuralPower, Warning,
			TEXT("[PWR] RCO SetEndpointIds rejected — not an id config target: %s"),
			*Buildable->GetName());
		return;
	}

	AStructuralPowerGraphSubsystem* Graph = GetAuthorityGraph(GetWorld());
	if (!Graph)
	{
		UE_LOG(LogStructuralPower, Warning, TEXT("[PWR] RCO SetEndpointIds — no graph subsystem"));
		return;
	}

	Graph->SetEndpointIds(Buildable, Source, Control, bClearSource, bClearControl);

	UE_LOG(LogStructuralPower, Log,
		TEXT("[PWR] RCO SetEndpointIds %s src=%s ctl=%s clearSrc=%d clearCtl=%d"),
		*Buildable->GetName(),
		bClearSource ? TEXT("(clear)") : *Source.ToString(),
		bClearControl ? TEXT("(clear)") : *Control.ToString(),
		bClearSource ? 1 : 0,
		bClearControl ? 1 : 0);
}

void UStructuralPowerRCO::Server_RequestComponentIdList_Implementation(AFGBuildable* ContextBuildable)
{
	FStructuralComponentIdList List;
	if (!IsValid(ContextBuildable)
		|| !FStructuralEligibilityRules::IsIdConfigTarget(ContextBuildable))
	{
		Client_ReceiveComponentIdList(List);
		return;
	}

	if (AStructuralPowerGraphSubsystem* Graph = GetAuthorityGraph(GetWorld()))
	{
		const FStructuralComponentKey Key = Graph->MakeComponentKeyForBuildable(ContextBuildable);
		Graph->CollectIdsOnComponent(Key, List);
		const EStructuralChannel Tag = FStructuralEligibilityRules::ClassifyBuildable(ContextBuildable);
		List.ResolvedSource = Graph->ResolveSource(ContextBuildable, Tag);
		List.ResolvedControl = Graph->ResolveControl(ContextBuildable, Tag);
		FStructuralEndpointOverrides Overrides;
		if (Graph->GetEndpointOverrides(ContextBuildable, Overrides))
		{
			List.SourceOverride = Overrides.SourceOverride;
			List.ControlOverride = Overrides.ControlOverride;
		}
	}

	Client_ReceiveComponentIdList(List);
}

void UStructuralPowerRCO::Client_ReceiveComponentIdList_Implementation(
	const FStructuralComponentIdList& List)
{
	FStructuralPowerIdPresenterFactory::Get().ApplyComponentIdList(List);

	UE_LOG(LogStructuralPower, Log,
		TEXT("[PWR] RCO ComponentIdList target=%s sources=%d controls=%d switchCtl=%d lightGrp=%d resolvedSrc=%s resolvedCtl=%s"),
		List.DefaultSourceId.IsNone() ? TEXT("?") : *List.DefaultSourceId.ToString(),
		List.NamedSourceIds.Num(),
		List.NamedControlIds.Num(),
		List.NamedSwitchControlIds.Num(),
		List.NamedLightGroupIds.Num(),
		List.ResolvedSource.IsNone() ? TEXT("-") : *List.ResolvedSource.ToString(),
		List.ResolvedControl.IsNone() ? TEXT("-") : *List.ResolvedControl.ToString());
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
