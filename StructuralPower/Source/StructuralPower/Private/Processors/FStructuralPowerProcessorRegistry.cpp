// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Processors/FStructuralPowerProcessorRegistry.h"

#include "Processors/FStructuralEndpointCatalog.h"
#include "Processors/IStructuralEndpointProcessor.h"

FStructuralPowerProcessorRegistry& FStructuralPowerProcessorRegistry::Get()
{
	static FStructuralPowerProcessorRegistry Instance;
	return Instance;
}

void FStructuralPowerProcessorRegistry::Initialize()
{
	FStructuralEndpointCatalog::Get().Initialize();
}

const IStructuralPowerProcessor* FStructuralPowerProcessorRegistry::Find(
	const EStructuralEndpointKind Kind) const
{
	return FStructuralEndpointCatalog::Get().Find(Kind);
}

IStructuralEndpointProcessor* FStructuralPowerProcessorRegistry::FindMutable(
	const EStructuralEndpointKind Kind)
{
	return FStructuralEndpointCatalog::Get().FindMutable(Kind);
}

const IStructuralEndpointProcessor* FStructuralPowerProcessorRegistry::FindForBuildable(
	const AFGBuildable* Buildable) const
{
	return FStructuralEndpointCatalog::Get().Classify(Buildable);
}
