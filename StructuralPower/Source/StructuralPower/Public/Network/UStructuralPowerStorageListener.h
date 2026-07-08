// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UStructuralPowerStorageListener.generated.h"

class AFGBuildablePowerStorage;
class AStructuralPowerGraphSubsystem;
class UFGCircuitConnectionComponent;

UCLASS()
class STRUCTURALPOWER_API UStructuralPowerStorageListener : public UActorComponent
{
	GENERATED_BODY()

public:
	void BindSubsystem(
		AStructuralPowerGraphSubsystem* Graph,
		AFGBuildablePowerStorage* Storage);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleConnectionChanged(UFGCircuitConnectionComponent* Connection);

	UPROPERTY()
	TWeakObjectPtr<AStructuralPowerGraphSubsystem> GraphSubsystem;

	UPROPERTY()
	TWeakObjectPtr<AFGBuildablePowerStorage> BoundStorage;

	TArray<TWeakObjectPtr<UFGCircuitConnectionComponent>> BoundConns;
};
