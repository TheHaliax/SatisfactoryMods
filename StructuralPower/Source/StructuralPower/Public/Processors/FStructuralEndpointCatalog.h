// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Routing/EStructuralChannel.h"

class AFGBuildable;
class IStructuralEndpointProcessor;

class STRUCTURALPOWER_API FStructuralEndpointCatalog
{
public:
	static FStructuralEndpointCatalog& Get();

	void Initialize();

	void RegisterProcessor(IStructuralEndpointProcessor& Processor);

	const IStructuralEndpointProcessor* Classify(const AFGBuildable* Buildable) const;
	const IStructuralEndpointProcessor* Find(EStructuralEndpointKind Kind) const;
	IStructuralEndpointProcessor* FindMutable(EStructuralEndpointKind Kind);

	void ForEachKind(TFunctionRef<void(const IStructuralEndpointProcessor&)> Visitor) const;
	int32 GetRegisteredKindCount() const { return ByKind.Num(); }

private:
	FStructuralEndpointCatalog() = default;

	TMap<EStructuralEndpointKind, IStructuralEndpointProcessor*> ByKind;
	bool bInitialized = false;
};
