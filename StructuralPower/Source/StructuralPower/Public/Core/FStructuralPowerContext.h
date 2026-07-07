// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Core/EAttachContext.h"

class AStructuralPowerGraphSubsystem;
class UWorld;

struct STRUCTURALPOWER_API FStructuralPowerContext
{
	FStructuralPowerContext(
		AStructuralPowerGraphSubsystem& InGraph,
		EAttachContext InAttachContext,
		int32 InSiteRoot = INDEX_NONE);

	AStructuralPowerGraphSubsystem& Graph();
	const AStructuralPowerGraphSubsystem& Graph() const;
	EAttachContext GetAttachContext() const { return AttachContext; }
	int32 GetSiteRoot() const { return SiteRoot; }

	int32 ResolveSiteRoot(int32 FallbackRoot) const;

	UWorld* GetWorld() const;

private:
	AStructuralPowerGraphSubsystem* GraphPtr = nullptr;
	EAttachContext AttachContext = EAttachContext::RuntimePlace;
	int32 SiteRoot = INDEX_NONE;
};
