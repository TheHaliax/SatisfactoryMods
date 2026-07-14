// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Graph/FStructuralEndpointTypes.h"

class AFGBuildable;
class FStructuralGraphSession;

class STRUCTURALPOWER_API FStructuralPowerRestitch
{
public:
	FStructuralPowerRestitch() = default;

	void Bind(FStructuralGraphSession* InSession);

	bool ShouldEndpointParticipateInRestitch(AFGBuildable* Host, EStructuralEndpointKind Kind);

	bool ShouldMeshEndpoints(
		AFGBuildable* HostA,
		AFGBuildable* HostB,
		int32 ComponentRoot) const;

private:
	FStructuralGraphSession* Session = nullptr;
};
