// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Routing/EStructuralChannel.h"

class AFGBuildable;
class AFGBuildableLightSource;
class AFGBuildableLightsControlPanel;
class UFGCircuitConnectionComponent;
class UFGPowerConnectionComponent;
struct FStructuralChannelKey;
struct FStructuralPanelPorts;

struct FStructuralConnectorPowerSnapshot
{
	int32 Circuit = INDEX_NONE;
	bool bConnected = false;
	bool bCarriesPower = false;
	bool bHasPower = false;
	bool bSuppliesPower = false;

	static FStructuralConnectorPowerSnapshot From(const UFGCircuitConnectionComponent* Connector);
	FString Format() const;
};

namespace FStructuralPowerTrace
{
	bool IsEnabled();

	FString FormatEffectiveIdForTrace(EStructuralChannel Tag, FName EffectiveId);
	FString FormatSourceForTrace(const FStructuralChannelKey& Key);
	FString FormatControlForTrace(const FStructuralChannelKey& Key);
	FStructuralChannelKey KeyForBuildable(AFGBuildable* Buildable);

	void LogHook(AFGBuildable* Buildable, const TCHAR* Hook, const TCHAR* Action, const TCHAR* Detail = nullptr);
	void LogPlacementSkip(
		AFGBuildable* Buildable,
		const TCHAR* Reason,
		ELogVerbosity::Type Verbosity = ELogVerbosity::Warning);
	void LogLinkOp(
		const TCHAR* Op,
		UFGCircuitConnectionComponent* A,
		UFGCircuitConnectionComponent* B,
		bool bSuccess,
		const TCHAR* Path,
		ELogVerbosity::Type Verbosity = ELogVerbosity::Log);

	void LogConnector(
		const TCHAR* Role,
		AFGBuildable* Owner,
		UFGCircuitConnectionComponent* Connector);

	void LogLightConsumer(
		AFGBuildableLightSource* Light,
		int32 Root,
		bool bParentValid,
		const FStructuralChannelKey& Key,
		UFGPowerConnectionComponent* Plug,
		const TCHAR* Path,
		int32 PanelSupplyReady = -1,
		int32 PanelDownstreamFed = -1);

	void LogPanelConsumer(
		AFGBuildableLightsControlPanel* Panel,
		int32 Root,
		const FStructuralChannelKey& Key,
		const FStructuralPanelPorts& Ports,
		bool bSupplyReady,
		int32 ControlledCount,
		const TCHAR* Context = TEXT("process"));
}
