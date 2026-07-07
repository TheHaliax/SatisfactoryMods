// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Connection/IStructuralConnectionPoint.h"

class AFGBuildableCircuitSwitch;
class AStructuralPowerGraphSubsystem;

class STRUCTURALPOWER_API FStructuralSwitchConnectionPoint final : public IStructuralConnectionPoint
{
public:
	FStructuralSwitchConnectionPoint(
		AStructuralPowerGraphSubsystem& InGraph,
		AFGBuildableCircuitSwitch* InSwitch);

	AFGBuildableCircuitSwitch* GetSwitch() const { return Switch.Get(); }

	virtual UFGPowerConnectionComponent* GetStructuralConnector() override;
	virtual FStructuralWallAnchor GetStructureAnchor() const override;
	virtual void OnWireOrGateChanged(EAttachContext AttachContext = EAttachContext::WireDelta) override;

private:
	bool ResolveTrackedRoot(FStructuralNodeId& OutSwitchId, int32& OutRoot);

	AStructuralPowerGraphSubsystem& Graph;
	TWeakObjectPtr<AFGBuildableCircuitSwitch> Switch;
};
