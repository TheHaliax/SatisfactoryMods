// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Graph/FStructuralEndpointTypes.h"

#include "Processors/IStructuralEndpointProcessor.h"

class AFGBuildable;

class STRUCTURALPOWER_API FStructuralPowerProcessorRegistry
{
public:
	static FStructuralPowerProcessorRegistry& Get();

	FStructuralPowerProcessorRegistry() = default;
	FStructuralPowerProcessorRegistry(const FStructuralPowerProcessorRegistry&) = delete;
	FStructuralPowerProcessorRegistry& operator=(const FStructuralPowerProcessorRegistry&) = delete;

	void Initialize();

	const IStructuralPowerProcessor* Find(EStructuralEndpointKind Kind) const;

	IStructuralEndpointProcessor* FindMutable(EStructuralEndpointKind Kind);

	const IStructuralEndpointProcessor* FindForBuildable(const AFGBuildable* Buildable) const;
};
