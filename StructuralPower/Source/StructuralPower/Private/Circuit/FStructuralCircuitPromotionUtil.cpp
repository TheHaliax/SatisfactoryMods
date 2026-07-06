// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Circuit/FStructuralCircuitPromotionUtil.h"

#include "Diagnostics/FStructuralPowerTrace.h"
#include "Engine/World.h"
#include "FGCircuitConnectionComponent.h"
#include "FGCircuitSubsystem.h"
#include "FGPowerConnectionComponent.h"

bool FStructuralCircuitPromotionUtil::ComponentCarriesPower(
	const UFGPowerConnectionComponent* Component)
{
	if (!IsValid(Component))
	{
		return false;
	}

	if (Component->GetCircuitID() != INDEX_NONE || Component->IsConnected())
	{
		return true;
	}

	return !Component->IsHidden() && Component->GetNumConnections() > 0;
}

bool FStructuralCircuitPromotionUtil::ConnectorSuppliesPower(
	const UFGPowerConnectionComponent* Component)
{
	return ComponentCarriesPower(Component) && Component->HasPower();
}

void FStructuralCircuitPromotionUtil::PromoteCircuitLink(
	UFGPowerConnectionComponent* A,
	UFGPowerConnectionComponent* B,
	ELogVerbosity::Type PromoteVerbosity)
{
	if (!IsValid(A) || !IsValid(B))
	{
		return;
	}

	const int32 CircuitA = A->GetCircuitID();
	const int32 CircuitB = B->GetCircuitID();
	if (CircuitA != INDEX_NONE && CircuitA == CircuitB)
	{
		return;
	}

	if (!ComponentCarriesPower(A) && !ComponentCarriesPower(B))
	{
		return;
	}

	if (UWorld* World = A->GetWorld())
	{
		if (AFGCircuitSubsystem* CircuitSubsystem = AFGCircuitSubsystem::Get(World))
		{
			CircuitSubsystem->ConnectComponents(A, B);
			FStructuralPowerTrace::LogLinkOp(
				TEXT("promote"),
				A,
				B,
				true,
				TEXT("ConnectComponents"),
				PromoteVerbosity);
		}
	}
}
