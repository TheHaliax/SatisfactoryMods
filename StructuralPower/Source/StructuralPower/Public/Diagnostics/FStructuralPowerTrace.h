// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class AFGBuildable;
class UFGCircuitConnectionComponent;
class UFGPowerConnectionComponent;

/** Deep placement/link trace — grep FactoryGame.log for [PWR]. Toggle: StructuralPower.Trace 1 */
namespace FStructuralPowerTrace
{
	bool IsEnabled();

	void LogHook(AFGBuildable* Buildable, const TCHAR* Hook, const TCHAR* Action, const TCHAR* Detail = nullptr);
	void LogPlacementSkip(AFGBuildable* Buildable, const TCHAR* Reason);
	void LogOverlapQuery(
		AFGBuildable* Source,
		int32 RawHits,
		int32 BusHits,
		int32 ModNeighborHits,
		int32 NoConnectorHits);
	void LogConnector(const TCHAR* Context, const UFGCircuitConnectionComponent* Connector);
	void LogLinkOp(
		const TCHAR* Op,
		UFGCircuitConnectionComponent* A,
		UFGCircuitConnectionComponent* B,
		bool bSuccess,
		const TCHAR* Path);
	void LogOutletBridge(
		AFGBuildable* Outlet,
		bool bSuccess,
		int32 StructureCircuitId,
		int32 OutletBusCircuitId,
		int32 WiredVisibleCount,
		const TCHAR* Note);
}
