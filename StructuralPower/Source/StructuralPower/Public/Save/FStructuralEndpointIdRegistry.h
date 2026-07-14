// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Core/FStructuralNodeId.h"
#include "Routing/EStructuralChannel.h"

class AFGBuildable;

class STRUCTURALPOWER_API FStructuralEndpointIdRegistry
{
public:
	FStructuralEndpointIdRegistry() = default;

	void Bind(
		TMap<FStructuralComponentKey, FName>& InComponentDefaultIds,
		TMap<FStructuralNodeId, FStructuralEndpointOverrides>& InPlayerEndpointOverrides,
		int32& InNextStructureDefaultIdIndex);

	FName GetOrCreateComponentDefaultId(const FStructuralComponentKey& ComponentKey);

	bool TryGetComponentDefaultId(const FStructuralComponentKey& ComponentKey, FName& OutId) const;

	void MigrateLegacyStructureDefaultIds();

	bool TryGetPlayerOverride(
		const FStructuralNodeId& NodeId,
		FStructuralEndpointOverrides& Out) const;

	const FStructuralEndpointOverrides* FindPlayerOverride(const FStructuralNodeId& NodeId) const;

	void ApplyPlayerOverrideEdits(
		const FStructuralNodeId& NodeId,
		FName Source,
		FName Control,
		bool bClearSource,
		bool bClearControl,
		bool bGlobalControl = false,
		bool bTouchGlobalControl = false);

	void ForEachPlayerOverride(
		TFunctionRef<void(const FStructuralNodeId&, const FStructuralEndpointOverrides&)> Visitor) const;

private:
	FName AllocNextStructureDefaultId();

	TMap<FStructuralComponentKey, FName>* ComponentDefaultIds = nullptr;
	TMap<FStructuralNodeId, FStructuralEndpointOverrides>* PlayerEndpointOverrides = nullptr;
	int32* NextStructureDefaultIdIndex = nullptr;
};
