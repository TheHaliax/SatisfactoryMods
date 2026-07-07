// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class AFGBuildableControlPanelHost;
class AFGBuildableLightsControlPanel;
class AFGBuildableLightSource;
class AStructuralPowerGraphSubsystem;
struct FStructuralChannelKey;
struct FStructuralPanelPorts;

class STRUCTURALPOWER_API FStructuralPanelAttach
{
public:
	static void TearDownLinks(AFGBuildableControlPanelHost* Panel, const FStructuralPanelPorts& Ports);

	static bool SupplyAlreadyLinked(
		AStructuralPowerGraphSubsystem& Graph,
		AFGBuildableLightsControlPanel* Panel,
		const FStructuralPanelPorts& Ports,
		int32 ComponentRoot,
		const FStructuralChannelKey& PanelKey);

	static bool TryLinkSupply(
		AStructuralPowerGraphSubsystem& Graph,
		AFGBuildableLightsControlPanel* Panel,
		const FStructuralPanelPorts& Ports,
		int32 ComponentRoot,
		const FStructuralChannelKey& PanelKey);

	static void RestitchDownstream(
		AStructuralPowerGraphSubsystem& Graph,
		AFGBuildableLightsControlPanel* Panel,
		const FStructuralPanelPorts& Ports,
		int32 ComponentRoot,
		FName PanelControl);
};
