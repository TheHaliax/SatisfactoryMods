// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Core/FStructuralNodeId.h"

class STRUCTURALPOWER_API FStructuralSiteState
{
public:
	void BeginEchoScope(const TArray<int32>& SiteRoots);
	void ClearEchoScope();

	bool IsEchoScopeActive() const;
	bool IsEchoDirtySite(int32 SiteRoot) const;

	bool WasPanelEchoProcessedInScope(const FStructuralNodeId& PanelId) const;
	void NotePanelEchoProcessed(const FStructuralNodeId& PanelId);

	bool WasPanelToggleHandledInScope(const FStructuralNodeId& PanelId) const;
	void NotePanelToggleHandled(const FStructuralNodeId& PanelId);

private:
	TSet<int32> EchoDirtySites;
	TSet<FStructuralNodeId> EchoProcessedPanelsInScope;
	TSet<FStructuralNodeId> EchoToggleHandledPanelsInScope;
};
