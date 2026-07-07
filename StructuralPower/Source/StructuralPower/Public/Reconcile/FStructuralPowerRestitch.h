// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Core/EAttachContext.h"
#include "Graph/FStructuralEndpointTypes.h"

class AFGBuildable;
class AStructuralPowerGraphSubsystem;

class STRUCTURALPOWER_API FStructuralPowerRestitch
{
public:
	FStructuralPowerRestitch() = default;

	void Bind(AStructuralPowerGraphSubsystem* InSubsystem);

	void RestitchLightEndpointsForRoot(int32 Root, EAttachContext AttachContext);
	void RestitchPanelEndpointsForRoot(int32 Root, EAttachContext AttachContext);
	void RestitchPanelsWithControlOnRoot(int32 Root, FName ControlId);
	void RestitchKeyedSubnetsAfterMeshFeedRefresh(int32 Root, EAttachContext AttachContext);
	void RestitchComponent(int32 Root, bool bTearDownFirst, EAttachContext AttachContext);
	void ReEnergizeComponentRoots(
		const TArray<int32>& Roots,
		bool bTearDownFirst,
		EAttachContext AttachContext);

	bool ShouldEndpointParticipateInRestitch(AFGBuildable* Host, EStructuralEndpointKind Kind);
	bool ShouldMeshEndpoints(AFGBuildable* HostA, AFGBuildable* HostB, int32 ComponentRoot) const;

private:
	AStructuralPowerGraphSubsystem* Subsystem = nullptr;
};
