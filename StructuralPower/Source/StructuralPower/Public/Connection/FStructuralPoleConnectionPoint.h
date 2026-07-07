// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Connection/IStructuralConnectionPoint.h"

class AFGBuildablePowerPole;
class AStructuralPowerGraphSubsystem;

class STRUCTURALPOWER_API FStructuralPoleConnectionPoint final : public IStructuralConnectionPoint
{
public:
	FStructuralPoleConnectionPoint(
		AStructuralPowerGraphSubsystem& InGraph,
		AFGBuildablePowerPole* InPole);

	AFGBuildablePowerPole* GetPole() const { return Pole.Get(); }

	virtual UFGPowerConnectionComponent* GetStructuralConnector() override;
	virtual FStructuralWallAnchor GetStructureAnchor() const override;
	virtual void OnWireOrGateChanged() override;

private:
	AStructuralPowerGraphSubsystem& Graph;
	TWeakObjectPtr<AFGBuildablePowerPole> Pole;
};
