// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Graph/FStructuralEndpointTypes.h"

class AFGBuildable;
class IStructuralPowerProcessor;

class STRUCTURALPOWER_API FStructuralPowerProcessorRegistry
{
public:
	static FStructuralPowerProcessorRegistry& Get();

	FStructuralPowerProcessorRegistry() = default;
	FStructuralPowerProcessorRegistry(const FStructuralPowerProcessorRegistry&) = delete;
	FStructuralPowerProcessorRegistry& operator=(const FStructuralPowerProcessorRegistry&) = delete;

	void Initialize();

	const IStructuralPowerProcessor* Find(EStructuralEndpointKind Kind) const;

	IStructuralPowerProcessor* FindMutable(EStructuralEndpointKind Kind);

	const IStructuralPowerProcessor* FindForBuildable(const AFGBuildable* Buildable) const;

private:
	void Register(TUniquePtr<IStructuralPowerProcessor> Processor);

	TMap<EStructuralEndpointKind, TUniquePtr<IStructuralPowerProcessor>> ByKind;
};
