// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Graph/FStructuralEndpointTypes.h"

class AFGBuildable;
class AFGBuildablePowerPole;
class FStructuralGraphSession;
struct FStructuralNodeId;
struct FStructuralOutletParentResolveParams;
struct FStructuralWallAnchor;
struct FTrackedEndpoint;

class STRUCTURALPOWER_API FStructuralBridgeRootIndex
{
public:
	FStructuralBridgeRootIndex() = default;

	void Bind(FStructuralGraphSession* InSession);

	void MarkBridgeEndpointRootIndexDirty();
	void RefreshBridgeEndpointRootIndex();
	void AddEndpointToRootIndex(
		int32 Root,
		EStructuralEndpointKind Kind,
		const FStructuralNodeId& EndpointId);

	int32 FindRootForTrackedEndpoint(const FTrackedEndpoint& Tracked) const;

	int32 ResolveBridgeRootFromAnchor(
		AFGBuildable* Host,
		const FStructuralWallAnchor& Anchor,
		FStructuralNodeId& OutParentId,
		bool bPreferBulkResolve);

	int32 ResolveBridgeComponentRootBulk(
		AFGBuildable* Host,
		const FStructuralWallAnchor& Anchor,
		FStructuralNodeId& OutParentId);

	int32 ResolvePoleComponentRoot(
		AFGBuildablePowerPole* Pole,
		const FStructuralWallAnchor& Anchor,
		FStructuralNodeId& OutParentId);

	int32 ResolveEndpointComponentRoot(
		AFGBuildable* Endpoint,
		const FStructuralWallAnchor& Anchor,
		FStructuralNodeId& OutParentId);

	int32 ResolveBridgeHostComponentRoot(
		AFGBuildable* Host,
		FStructuralNodeId* OutParentId = nullptr);

	int32 GetEndpointComponentRoot(AFGBuildable* Endpoint);

	bool EnsureParentRegisteredInGraph(
		const FStructuralWallAnchor& Anchor,
		FStructuralNodeId& OutParentId);

	FStructuralWallAnchor ResolveOutletAnchor(AFGBuildable* Outlet) const;
	FStructuralOutletParentResolveParams MakeOutletParentResolveParams() const;
	FStructuralNodeId MakeParentNodeId(const FStructuralWallAnchor& Anchor) const;

private:
	FStructuralGraphSession* Session = nullptr;
};
