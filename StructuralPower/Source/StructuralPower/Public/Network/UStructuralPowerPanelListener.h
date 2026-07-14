// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UStructuralPowerPanelListener.generated.h"

class AFGBuildableLightsControlPanel;
class AStructuralPowerGraphSubsystem;

UCLASS()
class STRUCTURALPOWER_API UStructuralPowerPanelListener : public UActorComponent
{
	GENERATED_BODY()

public:
	void BindSubsystem(
		AStructuralPowerGraphSubsystem* Graph,
		AFGBuildableLightsControlPanel* Panel);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleControlledBuildablesChanged();

	UFUNCTION()
	void HandleLightControlPanelStateChanged(bool bIsEnabled);

	UPROPERTY()
	TWeakObjectPtr<AStructuralPowerGraphSubsystem> GraphSubsystem;

	UPROPERTY()
	TWeakObjectPtr<AFGBuildableLightsControlPanel> BoundPanel;
};
