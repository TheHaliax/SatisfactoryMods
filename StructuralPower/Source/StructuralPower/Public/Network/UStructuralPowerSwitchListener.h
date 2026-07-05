// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UStructuralPowerSwitchListener.generated.h"

class AFGBuildableCircuitSwitch;
class AStructuralPowerGraphSubsystem;

/**
 * Binds native switch delegates to the graph subsystem. One instance per tracked switch;
 * lives on the switch actor so dynamic multicast delegates have a stable UObject target.
 */
UCLASS()
class STRUCTURALPOWER_API UStructuralPowerSwitchListener : public UActorComponent
{
	GENERATED_BODY()

public:
	void BindSubsystem(AStructuralPowerGraphSubsystem* Graph, AFGBuildableCircuitSwitch* Switch);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleSwitchOnChanged();

	UFUNCTION()
	void HandleCircuitsChanged();

	UPROPERTY()
	TWeakObjectPtr<AStructuralPowerGraphSubsystem> GraphSubsystem;

	UPROPERTY()
	TWeakObjectPtr<AFGBuildableCircuitSwitch> BoundSwitch;
};
