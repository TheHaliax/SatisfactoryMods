// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Routing/EStructuralChannel.h"

class AFGBuildableLightSource;
class AFGBuildableLightsControlPanel;
class FStructuralGraphSession;
struct FStructuralChannelKey;

class STRUCTURALPOWER_API FStructuralPowerReconcile
{
public:
	FStructuralPowerReconcile() = default;

	void Bind(FStructuralGraphSession* InSession);

	/** Panel/transfer arm after remesh — no phase decisions. */
	void RunPostLoadLightWorkers();
	void MaybeRunFinalLightingReconcile();
	void ReconcileAllPanelEndpoints();
	void ReconcileAllLightConsumers();
	void ReconcileGroupLightingState();
	void RefreshPanelsForLightSourceOnRoot(int32 Root, FName LightSource);
	void LogPanelReconcileSummary(AFGBuildableLightsControlPanel* Panel);

	void EnumerateTrackedLightsOnRoot(
		int32 Root,
		TFunctionRef<void(AFGBuildableLightSource*)> Visitor);

	bool IsDirectSwitchFedLight(int32 Root, const FStructuralChannelKey& LightKey);
	bool IsPanelDownstreamLight(int32 Root, const FStructuralChannelKey& LightKey);
	bool IsSwitchFeedOpen(int32 Root, FName SwitchControlId);

	void CollectKnownPanelEndpoints(TArray<AFGBuildableLightsControlPanel*>& OutPanels);
	void ApplyKeyedSubnetAllPanels();

private:
	void RefreshKeyedTransferAfterLoad();
	void RefreshNamedControlPanelsAfterLoad();
	bool LightingReconcileNeedsRetry();
	bool PanelNeedsLightingReconcileRetry(AFGBuildableLightsControlPanel* Panel) const;
	void SuspendAllIntegratedLighting();

	FStructuralGraphSession* Session = nullptr;
};
