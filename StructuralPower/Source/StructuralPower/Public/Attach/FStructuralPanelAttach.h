// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class AFGBuildableControlPanelHost;
class AFGBuildableLightsControlPanel;
class AFGBuildableLightSource;
class FStructuralGraphSession;
class UFGPowerConnectionComponent;
struct FStructuralChannelKey;
struct FStructuralPanelPorts;

class STRUCTURALPOWER_API FStructuralPanelAttach
{
public:
	static void TearDownLinks(AFGBuildableControlPanelHost* Panel, const FStructuralPanelPorts& Ports);

	static void TearDownDownstreamLinks(
		AFGBuildableControlPanelHost* Panel,
		const FStructuralPanelPorts& Ports);

	static bool SupplyAlreadyLinked(
		FStructuralGraphSession& Session,
		AFGBuildableLightsControlPanel* Panel,
		const FStructuralPanelPorts& Ports,
		int32 ComponentRoot,
		const FStructuralChannelKey& PanelKey);

	static bool TryLinkSupply(
		FStructuralGraphSession& Session,
		AFGBuildableLightsControlPanel* Panel,
		const FStructuralPanelPorts& Ports,
		int32 ComponentRoot,
		const FStructuralChannelKey& PanelKey,
		bool bMeshOnlyLinks = false);

	static void RestitchDownstream(
		FStructuralGraphSession& Session,
		AFGBuildableLightsControlPanel* Panel,
		const FStructuralPanelPorts& Ports,
		int32 ComponentRoot,
		FName PanelControl);

	static void PromotePanelDownstreamSubnet(
		FStructuralGraphSession& Session,
		AFGBuildableLightsControlPanel* Panel,
		const FStructuralPanelPorts& Ports,
		UFGPowerConnectionComponent* InputPower);
};
