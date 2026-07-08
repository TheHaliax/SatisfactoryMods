// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Core/FStructuralNodeId.h"
#include "Graph/EStructuralHostKind.h"
#include "Graph/FStructuralOutletParentResolver.h"
#include "Lightweight/FStructuralLightweightTypes.h"

struct FStructuralSiteContext
{
	FStructuralWallAnchor ParentAnchor;
	FStructuralNodeId ParentId;
	int32 SiteRoot = INDEX_NONE;
	EStructuralHostKind HostKind = EStructuralHostKind::Unknown;
	EStructuralOutletParentMethod ParentMethod = EStructuralOutletParentMethod::None;
	bool bAnchored = false;
};
