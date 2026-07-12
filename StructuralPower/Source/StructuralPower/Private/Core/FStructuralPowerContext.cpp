// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Core/FStructuralPowerContext.h"

#include "Core/FStructuralGraphSession.h"
#include "Engine/World.h"
#include "Save/AStructuralPowerGraphSubsystem.h"

FStructuralPowerContext::FStructuralPowerContext(
	AStructuralPowerGraphSubsystem& InGraph,
	FStructuralGraphSession& InSession,
	const EAttachContext InAttachContext,
	const int32 InSiteRoot)
	: GraphPtr(&InGraph)
	, SessionPtr(&InSession)
	, AttachContext(InAttachContext)
	, SiteRoot(InSiteRoot)
{
}

AStructuralPowerGraphSubsystem& FStructuralPowerContext::Graph()
{
	check(GraphPtr);
	return *GraphPtr;
}

const AStructuralPowerGraphSubsystem& FStructuralPowerContext::Graph() const
{
	check(GraphPtr);
	return *GraphPtr;
}

FStructuralGraphSession& FStructuralPowerContext::Session()
{
	check(SessionPtr);
	return *SessionPtr;
}

const FStructuralGraphSession& FStructuralPowerContext::Session() const
{
	check(SessionPtr);
	return *SessionPtr;
}

int32 FStructuralPowerContext::ResolveSiteRoot(const int32 FallbackRoot) const
{
	return SiteRoot != INDEX_NONE ? SiteRoot : FallbackRoot;
}

UWorld* FStructuralPowerContext::GetWorld() const
{
	return GraphPtr ? GraphPtr->GetWorld() : nullptr;
}
