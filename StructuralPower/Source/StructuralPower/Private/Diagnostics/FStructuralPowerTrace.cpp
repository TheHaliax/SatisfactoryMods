// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Diagnostics/FStructuralPowerTrace.h"

#include "Buildables/FGBuildable.h"
#include "Config/FStructuralPowerModConfig.h"
#include "FGCircuitConnectionComponent.h"
#include "FGPowerConnectionComponent.h"
#include "Routing/EStructuralChannel.h"
#include "Save/AStructuralPowerGraphSubsystem.h"
#include "StructuralPowerLog.h"

bool FStructuralPowerTrace::IsEnabled()
{
	return FStructuralPowerModConfig::IsTraceEnabled();
}

FString FStructuralPowerTrace::FormatEffectiveIdForTrace(EStructuralChannel Tag, FName EffectiveId)
{
	if (Tag == EStructuralChannel::Structure)
	{
		return TEXT("-");
	}

	return EffectiveId.IsNone() ? TEXT("?") : EffectiveId.ToString();
}

FStructuralChannelKey FStructuralPowerTrace::KeyForBuildable(AFGBuildable* Buildable)
{
	if (!IsValid(Buildable))
	{
		return {};
	}

	if (UWorld* World = Buildable->GetWorld())
	{
		if (AStructuralPowerGraphSubsystem* Graph = AStructuralPowerGraphSubsystem::Find(World))
		{
			return Graph->ResolveChannelKeyForBuildable(Buildable);
		}
	}

	return {};
}

void FStructuralPowerTrace::LogHook(
	AFGBuildable* Buildable,
	const TCHAR* Hook,
	const TCHAR* Action,
	const TCHAR* Detail)
{
	if (!IsValid(Buildable))
	{
		return;
	}

	if (IsEnabled())
	{
		const FStructuralChannelKey Key = KeyForBuildable(Buildable);
		UE_LOG(LogStructuralPower, Log,
			TEXT("[PWR] hook %s %s %s class=%s tag=%s id=%s detail=%s"),
			Hook ? Hook : TEXT("?"),
			*Buildable->GetName(),
			Action ? Action : TEXT("?"),
			*Buildable->GetClass()->GetName(),
			StructuralChannelToString(Key.Tag),
			*FormatEffectiveIdForTrace(Key.Tag, Key.EffectiveId),
			Detail ? Detail : TEXT(""));
		return;
	}

	UE_LOG(LogStructuralPower, Log,
		TEXT("[PWR] hook %s %s %s class=%s detail=%s"),
		Hook ? Hook : TEXT("?"),
		*Buildable->GetName(),
		Action ? Action : TEXT("?"),
		*Buildable->GetClass()->GetName(),
		Detail ? Detail : TEXT(""));
}

void FStructuralPowerTrace::LogPlacementSkip(AFGBuildable* Buildable, const TCHAR* Reason)
{
	if (!IsValid(Buildable))
	{
		return;
	}

	UE_LOG(LogStructuralPower, Warning,
		TEXT("[PWR] skip %s class=%s reason=%s"),
		*Buildable->GetName(),
		*Buildable->GetClass()->GetName(),
		Reason ? Reason : TEXT("?"));
}

void FStructuralPowerTrace::LogConnector(const TCHAR* Context, const UFGCircuitConnectionComponent* Connector)
{
	if (!IsEnabled() || !Context)
	{
		return;
	}

	if (!IsValid(Connector))
	{
		UE_LOG(LogStructuralPower, Log, TEXT("[PWR] %s connector=null"), Context);
		return;
	}

	const FStructuralChannelKey Key = KeyForBuildable(Cast<AFGBuildable>(Connector->GetOwner()));
	TArray<UFGCircuitConnectionComponent*> HiddenLinks;
	Connector->GetHiddenConnections(HiddenLinks);

	UE_LOG(LogStructuralPower, Log,
		TEXT("[PWR] %s hidden=%d circuitId=%d connected=%d hiddenLinks=%d owner=%s tag=%s id=%s"),
		Context,
		Connector->IsHidden() ? 1 : 0,
		Connector->GetCircuitID(),
		Connector->IsConnected() ? 1 : 0,
		HiddenLinks.Num(),
		*Connector->GetName(),
		StructuralChannelToString(Key.Tag),
		*FormatEffectiveIdForTrace(Key.Tag, Key.EffectiveId));
}

void FStructuralPowerTrace::LogLinkOp(
	const TCHAR* Op,
	UFGCircuitConnectionComponent* A,
	UFGCircuitConnectionComponent* B,
	bool bSuccess,
	const TCHAR* Path,
	ELogVerbosity::Type Verbosity)
{
	if (!IsEnabled() || !Op)
	{
		return;
	}

	const int32 CircuitA = IsValid(A) ? A->GetCircuitID() : INDEX_NONE;
	const int32 CircuitB = IsValid(B) ? B->GetCircuitID() : INDEX_NONE;
	const bool bHadLink = IsValid(A) && IsValid(B) && A->HasHiddenConnection(B);
	const FStructuralChannelKey KeyA = KeyForBuildable(Cast<AFGBuildable>(IsValid(A) ? A->GetOwner() : nullptr));
	const FStructuralChannelKey KeyB = KeyForBuildable(Cast<AFGBuildable>(IsValid(B) ? B->GetOwner() : nullptr));

	if (Verbosity == ELogVerbosity::Verbose)
	{
		UE_LOG(LogStructuralPower, Verbose,
			TEXT("[PWR] link %s ok=%d path=%s hadLink=%d A(circuit=%d tag=%s id=%s) B(circuit=%d tag=%s id=%s)"),
			Op,
			bSuccess ? 1 : 0,
			Path ? Path : TEXT("?"),
			bHadLink ? 1 : 0,
			CircuitA,
			StructuralChannelToString(KeyA.Tag),
			*FormatEffectiveIdForTrace(KeyA.Tag, KeyA.EffectiveId),
			CircuitB,
			StructuralChannelToString(KeyB.Tag),
			*FormatEffectiveIdForTrace(KeyB.Tag, KeyB.EffectiveId));
	}
	else
	{
		UE_LOG(LogStructuralPower, Log,
			TEXT("[PWR] link %s ok=%d path=%s hadLink=%d A(circuit=%d tag=%s id=%s) B(circuit=%d tag=%s id=%s)"),
			Op,
			bSuccess ? 1 : 0,
			Path ? Path : TEXT("?"),
			bHadLink ? 1 : 0,
			CircuitA,
			StructuralChannelToString(KeyA.Tag),
			*FormatEffectiveIdForTrace(KeyA.Tag, KeyA.EffectiveId),
			CircuitB,
			StructuralChannelToString(KeyB.Tag),
			*FormatEffectiveIdForTrace(KeyB.Tag, KeyB.EffectiveId));
	}
}
