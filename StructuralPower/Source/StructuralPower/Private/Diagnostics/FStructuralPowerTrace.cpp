// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Diagnostics/FStructuralPowerTrace.h"

#include "Buildables/FGBuildable.h"
#include "FGCircuitConnectionComponent.h"
#include "FGPowerConnectionComponent.h"
#include "HAL/IConsoleManager.h"
#include "StructuralPowerLog.h"

namespace
{
static TAutoConsoleVariable<int32> CVarStructuralPowerTrace(
	TEXT("StructuralPower.Trace"),
	0,
	TEXT("Deep propagation trace ([PWR] prefix). 0=off, 1=on."),
	ECVF_Default);
}

bool FStructuralPowerTrace::IsEnabled()
{
	return CVarStructuralPowerTrace.GetValueOnAnyThread() != 0;
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

	TArray<UFGCircuitConnectionComponent*> HiddenLinks;
	Connector->GetHiddenConnections(HiddenLinks);

	UE_LOG(LogStructuralPower, Log,
		TEXT("[PWR] %s hidden=%d circuitId=%d connected=%d hiddenLinks=%d owner=%s"),
		Context,
		Connector->IsHidden() ? 1 : 0,
		Connector->GetCircuitID(),
		Connector->IsConnected() ? 1 : 0,
		HiddenLinks.Num(),
		*Connector->GetName());
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

	if (Verbosity == ELogVerbosity::Verbose)
	{
		UE_LOG(LogStructuralPower, Verbose,
			TEXT("[PWR] link %s ok=%d path=%s hadLink=%d A(circuit=%d) B(circuit=%d)"),
			Op,
			bSuccess ? 1 : 0,
			Path ? Path : TEXT("?"),
			bHadLink ? 1 : 0,
			CircuitA,
			CircuitB);
	}
	else
	{
		UE_LOG(LogStructuralPower, Log,
			TEXT("[PWR] link %s ok=%d path=%s hadLink=%d A(circuit=%d) B(circuit=%d)"),
			Op,
			bSuccess ? 1 : 0,
			Path ? Path : TEXT("?"),
			bHadLink ? 1 : 0,
			CircuitA,
			CircuitB);
	}
}
