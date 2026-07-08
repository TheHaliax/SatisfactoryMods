// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Graph/EStructuralHostKind.h"
#include "Graph/FStructuralOutletParentResolver.h"

class AFGBuildable;

class STRUCTURALPOWER_API FStructuralHostAttachAdapter
{
public:
	static EStructuralHostKind ClassifyHost(const FStructuralWallAnchor& Anchor);

	static bool ConfirmSiteAttach(
		const FStructuralOutletParentResolveResult& ParentResolve,
		AFGBuildable* Endpoint);

	static FBox ExpandHostBounds(
		const FBox& Bounds,
		TSubclassOf<AFGBuildable> HostClass,
		EStructuralHostKind HostKind);
};
