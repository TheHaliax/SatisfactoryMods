// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Core/FStructuralNodeId.h"
#include "Lightweight/FStructuralLightweightTypes.h"

class AFGBuildable;
class UWorld;
class FStructuralBusMemberSpatialIndex;
class FStructuralConnectivityGraph;
class FStructuralLightweightIndex;

enum class EStructuralOutletParentMethod : uint8
{
	None,
	ActorParent,
	LightweightFaceAttach,
	LightweightIndex,
	StructureGraph,
	LiveScan
};

struct FStructuralOutletParentResolveResult
{
	FStructuralWallAnchor Anchor;
	EStructuralOutletParentMethod Method = EStructuralOutletParentMethod::None;
};

struct FStructuralOutletParentResolveParams
{
	const FStructuralLightweightIndex* LightweightIndex = nullptr;
	const FStructuralBusMemberSpatialIndex* BusMemberIndex = nullptr;
	const FStructuralConnectivityGraph* StructureGraph = nullptr;
	TFunction<AFGBuildable*(const FStructuralNodeId&)> ResolveActorFromNodeId;
	bool bAllowLiveScan = true;
};

/** Chain: actor parent -> LW face attach -> spatial index -> graph bounds -> live scan. */
class STRUCTURALPOWER_API FStructuralOutletParentResolver
{
public:
	static FStructuralWallAnchor Resolve(
		AFGBuildable* Outlet,
		UWorld* World,
		const FStructuralLightweightIndex& LightweightIndex);

	static FStructuralWallAnchor Resolve(
		AFGBuildable* Outlet,
		UWorld* World,
		const FStructuralOutletParentResolveParams& Params);

	static FStructuralOutletParentResolveResult ResolveDetailed(
		AFGBuildable* Outlet,
		UWorld* World,
		const FStructuralLightweightIndex& LightweightIndex);

	static FStructuralOutletParentResolveResult ResolveDetailed(
		AFGBuildable* Outlet,
		UWorld* World,
		const FStructuralOutletParentResolveParams& Params);
};
