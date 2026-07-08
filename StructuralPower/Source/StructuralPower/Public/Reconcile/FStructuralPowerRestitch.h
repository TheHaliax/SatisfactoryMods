// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Graph/FStructuralEndpointTypes.h"

class AFGBuildable;
class AStructuralPowerGraphSubsystem;

class STRUCTURALPOWER_API FStructuralPowerRestitch
{
public:
	FStructuralPowerRestitch() = default;

	void Bind(AStructuralPowerGraphSubsystem* InSubsystem);

	bool ShouldEndpointParticipateInRestitch(AFGBuildable* Host, EStructuralEndpointKind Kind);

	bool ShouldMeshEndpoints(
		AFGBuildable* HostA,
		AFGBuildable* HostB,
		int32 ComponentRoot) const;

private:
	AStructuralPowerGraphSubsystem* Subsystem = nullptr;
};
