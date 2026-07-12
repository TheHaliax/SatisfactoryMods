// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Core/EStructuralPowerRole.h"
#include "Graph/FStructuralEndpointDescriptor.h"
#include "Graph/FStructuralEndpointTypes.h"

class AFGBuildable;
struct FStructuralPowerContext;

class STRUCTURALPOWER_API IStructuralEndpointProcessor
{
public:
	virtual ~IStructuralEndpointProcessor() = default;

	virtual const FStructuralEndpointDescriptor& GetDescriptor() const = 0;
	virtual EStructuralEndpointKind GetBuildableKind() const = 0;
	virtual EStructuralPowerRole GetPowerRole() const = 0;

	virtual void ProcessPlacement(
		FStructuralPowerContext& Ctx,
		AFGBuildable* Host,
		bool bLocalPromoteOnly = false) = 0;

	void Process(
		FStructuralPowerContext& Ctx,
		AFGBuildable* Host,
		bool bLocalPromoteOnly = false)
	{
		ProcessPlacement(Ctx, Host, bLocalPromoteOnly);
	}

	virtual void OnWireDelta(FStructuralPowerContext& Ctx, AFGBuildable* Host) {}
	virtual void OnCircuitsRebuilt(FStructuralPowerContext& Ctx, AFGBuildable* Host) {}
	virtual void OnToggleChanged(FStructuralPowerContext& Ctx, AFGBuildable* Host) {}
	virtual void RestitchOnRoot(FStructuralPowerContext& Ctx, int32 Root) {}
	virtual void TearDown(FStructuralPowerContext& Ctx, AFGBuildable* Host) {}
};

using IStructuralPowerProcessor = IStructuralEndpointProcessor;
