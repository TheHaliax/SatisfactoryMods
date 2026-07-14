// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Processors/FStructuralEndpointCatalog.h"

#include "Processors/FStructuralEndpointProcessors.h"
#include "Processors/IStructuralEndpointProcessor.h"
#include "Graph/FStructuralEndpointDescriptor.h"

FStructuralEndpointCatalog& FStructuralEndpointCatalog::Get()
{
	static FStructuralEndpointCatalog Instance;
	return Instance;
}

void FStructuralEndpointCatalog::Initialize()
{
	if (bInitialized)
	{
		return;
	}

	FStructuralEndpointProcessors::RegisterAll(*this);
	bInitialized = true;
}

void FStructuralEndpointCatalog::RegisterProcessor(IStructuralEndpointProcessor& Processor)
{
	ByKind.Add(Processor.GetBuildableKind(), &Processor);
}

const IStructuralEndpointProcessor* FStructuralEndpointCatalog::Classify(const AFGBuildable* Buildable) const
{
	if (!IsValid(Buildable))
	{
		return nullptr;
	}

	for (const TPair<EStructuralEndpointKind, IStructuralEndpointProcessor*>& Pair : ByKind)
	{
		const FStructuralEndpointDescriptor& Descriptor = Pair.Value->GetDescriptor();
		if (Descriptor.Classifier && Descriptor.Classifier(Buildable))
		{
			return Pair.Value;
		}
	}

	return nullptr;
}

const IStructuralEndpointProcessor* FStructuralEndpointCatalog::Find(const EStructuralEndpointKind Kind) const
{
	if (const IStructuralEndpointProcessor* const* Found = ByKind.Find(Kind))
	{
		return *Found;
	}
	return nullptr;
}

IStructuralEndpointProcessor* FStructuralEndpointCatalog::FindMutable(const EStructuralEndpointKind Kind)
{
	if (IStructuralEndpointProcessor** Found = ByKind.Find(Kind))
	{
		return *Found;
	}
	return nullptr;
}

void FStructuralEndpointCatalog::ForEachKind(
	const TFunctionRef<void(const IStructuralEndpointProcessor&)> Visitor) const
{
	for (const TPair<EStructuralEndpointKind, IStructuralEndpointProcessor*>& Pair : ByKind)
	{
		if (Pair.Value)
		{
			Visitor(*Pair.Value);
		}
	}
}
