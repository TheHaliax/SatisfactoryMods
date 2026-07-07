// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Core/EStructuralPowerRole.h"
#include "Graph/FStructuralEndpointTypes.h"

class AFGBuildable;
struct FStructuralPowerContext;

class STRUCTURALPOWER_API IStructuralPowerProcessor
{
public:
	virtual ~IStructuralPowerProcessor() = default;

	virtual EStructuralEndpointKind GetBuildableKind() const = 0;
	virtual EStructuralPowerRole GetPowerRole() const = 0;

	virtual void Process(
		FStructuralPowerContext& Ctx,
		AFGBuildable* Buildable,
		bool bLocalPromoteOnly = false) = 0;

	virtual void OnWireDelta(FStructuralPowerContext& Ctx, AFGBuildable* Buildable) {}

	virtual void RestitchOnRoot(FStructuralPowerContext& Ctx, int32 Root) {}

	virtual void TearDown(FStructuralPowerContext& Ctx, AFGBuildable* Buildable) {}
};
