// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "UStructuralPowerMachineWireListener.generated.h"

class AFGBuildable;
class AStructuralPowerGraphSubsystem;
class UFGCircuitConnectionComponent;

/**
 * Gen / extractor / mfg / transport / pump — re-run wire-delta Process when
 * vanilla plugs gain/lose wires (mirrors storage listener).
 */
UCLASS()
class STRUCTURALPOWER_API UStructuralPowerMachineWireListener : public UActorComponent {
  GENERATED_BODY()

 public:
  void BindSubsystem(AStructuralPowerGraphSubsystem* Graph, AFGBuildable* Host);

  static void Ensure(AStructuralPowerGraphSubsystem& Graph, AFGBuildable* Host);

 protected:
  virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

 private:
  void HandleConnectionChanged(UFGCircuitConnectionComponent* Connection);

  UPROPERTY()
  TWeakObjectPtr<AStructuralPowerGraphSubsystem> GraphSubsystem;

  UPROPERTY()
  TWeakObjectPtr<AFGBuildable> BoundHost;

  TArray<TWeakObjectPtr<UFGCircuitConnectionComponent>> BoundConns;
};
