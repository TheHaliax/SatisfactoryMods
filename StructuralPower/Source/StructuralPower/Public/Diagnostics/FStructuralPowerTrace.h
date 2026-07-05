// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Routing/EStructuralChannel.h"

class AFGBuildable;
class UFGCircuitConnectionComponent;
class UFGPowerConnectionComponent;

/** Deep placement/link trace — grep FactoryGame.log for [PWR]. Toggle: StructuralPower.Trace 1 (default off). */
namespace FStructuralPowerTrace
{
	bool IsEnabled();

	FString FormatEffectiveIdForTrace(EStructuralChannel Tag, FName EffectiveId);
	FStructuralChannelKey KeyForBuildable(AFGBuildable* Buildable);

	void LogHook(AFGBuildable* Buildable, const TCHAR* Hook, const TCHAR* Action, const TCHAR* Detail = nullptr);
	void LogPlacementSkip(AFGBuildable* Buildable, const TCHAR* Reason);
	void LogConnector(const TCHAR* Context, const UFGCircuitConnectionComponent* Connector);
	void LogLinkOp(
		const TCHAR* Op,
		UFGCircuitConnectionComponent* A,
		UFGCircuitConnectionComponent* B,
		bool bSuccess,
		const TCHAR* Path,
		ELogVerbosity::Type Verbosity = ELogVerbosity::Log);
}
