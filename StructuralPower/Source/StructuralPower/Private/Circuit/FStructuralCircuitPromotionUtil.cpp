// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Circuit/FStructuralCircuitPromotionUtil.h"

#include "Diagnostics/FStructuralPowerTrace.h"
#include "Diagnostics/FStructuralPowerTraceScope.h"
#include "Engine/World.h"
#include "FGCircuitConnectionComponent.h"
#include "FGCircuitSubsystem.h"
#include "FGPowerConnectionComponent.h"

bool FStructuralCircuitPromotionUtil::ComponentOnCircuit(
    const UFGPowerConnectionComponent* Component) {
  return IsValid(Component) && Component->GetCircuitID() != INDEX_NONE;
}

bool FStructuralCircuitPromotionUtil::ComponentCarriesPower(
    const UFGPowerConnectionComponent* Component) {
  if (!IsValid(Component)) {
    return false;
  }

  if (ComponentOnCircuit(Component)) {
    return true;
  }

  return !Component->IsHidden() && Component->GetNumConnections() > 0;
}

bool FStructuralCircuitPromotionUtil::ConnectorSuppliesPower(
    const UFGPowerConnectionComponent* Component) {
  // FG: HasPower() reflects live production on the connector's circuit.
  return IsValid(Component) && ComponentOnCircuit(Component) && Component->HasPower();
}

bool FStructuralCircuitPromotionUtil::HiddenLinkNeedsCircuitRepair(UFGPowerConnectionComponent* A,
                                                                   UFGPowerConnectionComponent* B) {
  if (!IsValid(A) || !IsValid(B)) {
    return false;
  }

  if (!A->HasHiddenConnection(B)) {
    return false;
  }

  const int32 CircuitA = A->GetCircuitID();
  const int32 CircuitB = B->GetCircuitID();
  if (CircuitA != INDEX_NONE && CircuitA == CircuitB) {
    return false;
  }

  return ComponentCarriesPower(A) || ComponentCarriesPower(B);
}

void FStructuralCircuitPromotionUtil::PromoteCircuitLink(UFGPowerConnectionComponent* A,
                                                         UFGPowerConnectionComponent* B,
                                                         ELogVerbosity::Type PromoteVerbosity) {
  HALSP_TRACE_SCOPE(TEXT("mod"), TEXT("circuit.PromoteCircuitLink"));

  if (!HiddenLinkNeedsCircuitRepair(A, B)) {
    return;
  }

  if (UWorld* World = A->GetWorld()) {
    if (AFGCircuitSubsystem* CircuitSubsystem = AFGCircuitSubsystem::Get(World)) {
      CircuitSubsystem->ConnectComponents(A, B);
      FStructuralPowerTrace::LogLinkOp(TEXT("promote"), A, B, true,
                                       TEXT("ConnectComponents_repair"), PromoteVerbosity);
    }
  }
}

void FStructuralCircuitPromotionUtil::DemoteHiddenCircuitLink(UFGPowerConnectionComponent* A,
                                                              UFGPowerConnectionComponent* B) {
  if (!IsValid(A) || !IsValid(B) || !A->HasHiddenConnection(B)) {
    return;
  }

  if (UWorld* World = A->GetWorld()) {
    if (AFGCircuitSubsystem* CircuitSubsystem = AFGCircuitSubsystem::Get(World)) {
      const int32 CircuitA = A->GetCircuitID();
      const int32 CircuitB = B->GetCircuitID();
      if (CircuitA != INDEX_NONE && CircuitA == CircuitB) {
        CircuitSubsystem->DisconnectComponents(A, B);
        FStructuralPowerTrace::LogLinkOp(TEXT("demote"), A, B, true, TEXT("DisconnectComponents"),
                                         ELogVerbosity::Verbose);
      }
    }
  }

  A->RemoveHiddenConnection(B);
  B->RemoveHiddenConnection(A);
}
