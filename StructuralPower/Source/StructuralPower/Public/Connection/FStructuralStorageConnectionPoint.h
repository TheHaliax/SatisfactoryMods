// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Connection/IStructuralConnectionPoint.h"

class AFGBuildablePowerStorage;
class AStructuralPowerGraphSubsystem;

class STRUCTURALPOWER_API FStructuralStorageConnectionPoint final : public IStructuralConnectionPoint
{
public:
	FStructuralStorageConnectionPoint(
		AStructuralPowerGraphSubsystem& InGraph,
		AFGBuildablePowerStorage* InStorage);

	AFGBuildablePowerStorage* GetStorage() const { return Storage.Get(); }

	virtual UFGPowerConnectionComponent* GetStructuralConnector() override;
	virtual FStructuralWallAnchor GetStructureAnchor() const override;
	virtual void OnWireOrGateChanged(EAttachContext AttachContext = EAttachContext::WireDelta) override;

private:
	AStructuralPowerGraphSubsystem& Graph;
	TWeakObjectPtr<AFGBuildablePowerStorage> Storage;
};
