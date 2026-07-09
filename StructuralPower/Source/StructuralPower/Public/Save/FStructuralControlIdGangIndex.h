// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Core/FStructuralNodeId.h"
#include "Routing/EStructuralChannel.h"

struct FStructuralControlGangKey
{
	FStructuralComponentKey ComponentKey;
	FName ControlId = NAME_None;

	bool IsValid() const { return ComponentKey.IsValid() && !ControlId.IsNone(); }

	bool operator==(const FStructuralControlGangKey& Other) const
	{
		return ComponentKey == Other.ComponentKey && ControlId == Other.ControlId;
	}

	friend uint32 GetTypeHash(const FStructuralControlGangKey& Key)
	{
		return HashCombine(GetTypeHash(Key.ComponentKey), GetTypeHash(Key.ControlId));
	}
};

class STRUCTURALPOWER_API FStructuralControlIdGangIndex
{
public:
	void ClearComponent(const FStructuralComponentKey& ComponentKey);

	void RegisterPublisher(const FStructuralNodeId& NodeId, const FStructuralControlGangKey& GangKey);

	void RemoveNode(const FStructuralNodeId& NodeId);

	TArray<FStructuralNodeId> GetGangMembers(
		const FStructuralComponentKey& ComponentKey,
		FName ControlId) const;

	int32 GetGangMemberCount(
		const FStructuralComponentKey& ComponentKey,
		FName ControlId) const;

private:
	TMap<FStructuralControlGangKey, TArray<FStructuralNodeId>> GangsByControlId;
	TMap<FStructuralNodeId, FStructuralControlGangKey> NodeToGangKey;
};
