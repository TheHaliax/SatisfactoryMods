// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Core/FStructuralNodeId.h"
#include "Routing/EStructuralChannel.h"

class AFGBuildable;
class FStructuralGraphSession;
struct FStructuralChannelKey;
struct FStructuralComponentIdList;
struct FStructuralComponentKey;
struct FStructuralEndpointOverrides;

class STRUCTURALPOWER_API FStructuralGraphIdOps
{
public:
	FStructuralGraphIdOps() = default;

	void Bind(FStructuralGraphSession* InSession);

	FStructuralComponentKey MakeComponentKeyForRoot(int32 ComponentRoot) const;
	FStructuralComponentKey MakeComponentKeyForParent(const FStructuralNodeId& ParentId) const;
	FStructuralComponentKey MakeComponentKeyForBuildable(const AFGBuildable* Buildable) const;

	bool GetEndpointOverrides(const AFGBuildable* Buildable, FStructuralEndpointOverrides& Out) const;
	FName GetOrCreateComponentDefaultId(const FStructuralComponentKey& ComponentKey);
	FName ResolveSource(AFGBuildable* Buildable, EStructuralChannel Tag);
	FName ResolveControl(AFGBuildable* Buildable, EStructuralChannel Tag);
	FStructuralChannelKey ResolveChannelKeyForBuildable(AFGBuildable* Buildable);

	void SetEndpointIds(
		AFGBuildable* Buildable,
		FName Source,
		FName Control,
		bool bClearSource,
		bool bClearControl,
		bool bGlobalControl = false,
		bool bTouchGlobalControl = false);

	bool CollectIdsOnComponent(const FStructuralComponentKey& Key, FStructuralComponentIdList& Out) const;

	void RebuildControlIdGangsForRoot(int32 ComponentRoot);

private:
	FStructuralGraphSession* Session = nullptr;
};
