// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Connection/IStructuralConnectionPoint.h"

class AFGBuildableLightsControlPanel;
class AStructuralPowerGraphSubsystem;

class STRUCTURALPOWER_API FStructuralPanelConnectionPoint final : public IStructuralConnectionPoint
{
public:
	FStructuralPanelConnectionPoint(
		AStructuralPowerGraphSubsystem& InGraph,
		AFGBuildableLightsControlPanel* InPanel);

	AFGBuildableLightsControlPanel* GetPanel() const { return Panel.Get(); }

	virtual UFGPowerConnectionComponent* GetStructuralConnector() override;
	virtual FStructuralWallAnchor GetStructureAnchor() const override;
	virtual void OnWireOrGateChanged(EAttachContext AttachContext = EAttachContext::WireDelta) override;

private:
	AStructuralPowerGraphSubsystem& Graph;
	TWeakObjectPtr<AFGBuildableLightsControlPanel> Panel;
};
