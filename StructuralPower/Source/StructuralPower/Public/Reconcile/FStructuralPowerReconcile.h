// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Routing/EStructuralChannel.h"

class AFGBuildableLightSource;
class AFGBuildableLightsControlPanel;
class AStructuralPowerGraphSubsystem;
struct FStructuralChannelKey;

class STRUCTURALPOWER_API FStructuralPowerReconcile
{
	friend class AStructuralPowerGraphSubsystem;

public:
	FStructuralPowerReconcile() = default;

	void Bind(AStructuralPowerGraphSubsystem* InSubsystem);

	void MaybeRunPostLoadLightReconcile();
	void ReconcileAllPanelEndpoints();
	void ReconcileAllLightConsumers();
	void LogPanelReconcileSummary(AFGBuildableLightsControlPanel* Panel);

	void EnumerateTrackedLightsOnRoot(
		int32 Root,
		TFunctionRef<void(AFGBuildableLightSource*)> Visitor);

	bool IsDirectSwitchFedLight(int32 Root, const FStructuralChannelKey& LightKey);
	bool IsPanelDownstreamLight(int32 Root, const FStructuralChannelKey& LightKey);
	bool IsSwitchFeedOpen(int32 Root, FName SwitchControlId);

private:
	void CollectKnownPanelEndpoints(TArray<AFGBuildableLightsControlPanel*>& OutPanels);
	void ApplyKeyedSubnetAllPanels();

	AStructuralPowerGraphSubsystem* Subsystem = nullptr;
};
