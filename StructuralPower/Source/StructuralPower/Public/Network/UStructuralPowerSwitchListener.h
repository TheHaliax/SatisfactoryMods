// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "UStructuralPowerSwitchListener.generated.h"

class AFGBuildableCircuitSwitch;
class AStructuralPowerGraphSubsystem;

UCLASS()
class STRUCTURALPOWER_API UStructuralPowerSwitchListener : public UActorComponent {
  GENERATED_BODY()

 public:
  void BindSubsystem(AStructuralPowerGraphSubsystem* Graph, AFGBuildableCircuitSwitch* Switch);
  void SyncSubscriptions(AStructuralPowerGraphSubsystem* Graph, AFGBuildableCircuitSwitch* Switch);

 protected:
  virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

 private:
  UFUNCTION()
  void HandleSwitchOnChanged();

  UPROPERTY()
  TWeakObjectPtr<AStructuralPowerGraphSubsystem> GraphSubsystem;

  UPROPERTY()
  TWeakObjectPtr<AFGBuildableCircuitSwitch> BoundSwitch;

  bool bToggleBound = false;
};
