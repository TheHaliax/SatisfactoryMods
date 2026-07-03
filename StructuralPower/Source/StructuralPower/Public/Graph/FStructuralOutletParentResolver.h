// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Lightweight/FStructuralLightweightTypes.h"

class AFGBuildable;
class UWorld;
class FStructuralLightweightIndex;

enum class EStructuralOutletParentMethod : uint8
{
	None,
	ActorParent,
	LightweightFaceAttach,
	LightweightIndex,
	LiveScan
};

struct FStructuralOutletParentResolveResult
{
	FStructuralWallAnchor Anchor;
	EStructuralOutletParentMethod Method = EStructuralOutletParentMethod::None;
};

/** Chain: actor parent -> LW face attach -> spatial index -> live scan. */
class STRUCTURALPOWER_API FStructuralOutletParentResolver
{
public:
	static FStructuralWallAnchor Resolve(
		AFGBuildable* Outlet,
		UWorld* World,
		const FStructuralLightweightIndex& LightweightIndex);

	static FStructuralOutletParentResolveResult ResolveDetailed(
		AFGBuildable* Outlet,
		UWorld* World,
		const FStructuralLightweightIndex& LightweightIndex);
};
