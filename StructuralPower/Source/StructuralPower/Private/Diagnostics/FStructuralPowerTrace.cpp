// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Diagnostics/FStructuralPowerTrace.h"

#include "Buildables/FGBuildable.h"
#include "Config/FStructuralPowerModConfig.h"
#include "FGCircuitConnectionComponent.h"
#include "FGPowerConnectionComponent.h"
#include "Routing/EStructuralChannel.h"
#include "Routing/FStructuralPowerRouter.h"
#include "StructuralPowerConstants.h"
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

FString FStructuralPowerTrace::FormatSourceForTrace(const FStructuralChannelKey& Key)
{
	if (Key.Tag == EStructuralChannel::Structure)
	{
		return TEXT("-");
	}

	if (FStructuralPowerRouter::UsesSourceControlModel(Key.Tag))
	{
		return Key.Source.IsNone() ? TEXT("?") : Key.Source.ToString();
	}

	return FormatEffectiveIdForTrace(Key.Tag, Key.EffectiveId);
}

FString FStructuralPowerTrace::FormatControlForTrace(const FStructuralChannelKey& Key)
{
	if (!FStructuralPowerRouter::UsesSourceControlModel(Key.Tag))
	{
		return TEXT("-");
	}

	if (Key.Tag == EStructuralChannel::Switch
		&& Key.Control == StructuralPowerConstants::ControlBypass)
	{
		return TEXT("BYPASS");
	}

	if (Key.Control.IsNone())
	{
		return TEXT("-");
	}

	if (Key.Control == StructuralPowerConstants::ControlUnconfigured)
	{
		return TEXT("UNCONFIGURED");
	}

	return Key.Control.ToString();
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
			TEXT("[PWR] hook %s %s %s class=%s tag=%s source=%s control=%s detail=%s"),
			Hook ? Hook : TEXT("?"),
			*Buildable->GetName(),
			Action ? Action : TEXT("?"),
			*Buildable->GetClass()->GetName(),
			StructuralChannelToString(Key.Tag),
			*FormatSourceForTrace(Key),
			*FormatControlForTrace(Key),
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
			TEXT("[PWR] link %s ok=%d path=%s hadLink=%d A(circuit=%d tag=%s src=%s ctl=%s)"
				" B(circuit=%d tag=%s src=%s ctl=%s)"),
			Op,
			bSuccess ? 1 : 0,
			Path ? Path : TEXT("?"),
			bHadLink ? 1 : 0,
			CircuitA,
			StructuralChannelToString(KeyA.Tag),
			*FormatSourceForTrace(KeyA),
			*FormatControlForTrace(KeyA),
			CircuitB,
			StructuralChannelToString(KeyB.Tag),
			*FormatSourceForTrace(KeyB),
			*FormatControlForTrace(KeyB));
	}
	else
	{
		UE_LOG(LogStructuralPower, Log,
			TEXT("[PWR] link %s ok=%d path=%s hadLink=%d A(circuit=%d tag=%s src=%s ctl=%s)"
				" B(circuit=%d tag=%s src=%s ctl=%s)"),
			Op,
			bSuccess ? 1 : 0,
			Path ? Path : TEXT("?"),
			bHadLink ? 1 : 0,
			CircuitA,
			StructuralChannelToString(KeyA.Tag),
			*FormatSourceForTrace(KeyA),
			*FormatControlForTrace(KeyA),
			CircuitB,
			StructuralChannelToString(KeyB.Tag),
			*FormatSourceForTrace(KeyB),
			*FormatControlForTrace(KeyB));
	}
}
