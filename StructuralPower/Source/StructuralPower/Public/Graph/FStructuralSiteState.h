// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "Core/FStructuralNodeId.h"
#include "CoreMinimal.h"
#include "Graph/FStructuralSiteFeedSignature.h"

class STRUCTURALPOWER_API FStructuralSiteState {
 public:
  void BeginEchoScope(const TArray<int32>& SiteRoots);
  void ClearEchoScope();

  bool IsEchoScopeActive() const;
  bool IsEchoDirtySite(int32 SiteRoot) const;

  bool WasPanelEchoProcessedInScope(const FStructuralNodeId& PanelId) const;
  void NotePanelEchoProcessed(const FStructuralNodeId& PanelId);

  bool WasPanelToggleHandledInScope(const FStructuralNodeId& PanelId) const;
  void NotePanelToggleHandled(const FStructuralNodeId& PanelId);

  bool WasSwitchEchoProcessedInScope(const FStructuralNodeId& SwitchId) const;
  void NoteSwitchEchoProcessed(const FStructuralNodeId& SwitchId);

  bool WasSwitchToggleHandledInScope(const FStructuralNodeId& SwitchId) const;
  void NoteSwitchToggleHandled(const FStructuralNodeId& SwitchId);

  void ClearFeedSignatures();
  void SeedFeedSignature(int32 Site, const FStructuralSiteFeedSignature& Signature);
  bool TryGetFeedSignature(int32 Site, FStructuralSiteFeedSignature& OutSignature) const;
  void SetFeedSignature(int32 Site, const FStructuralSiteFeedSignature& Signature);

 private:
  TSet<int32> EchoDirtySites;
  TSet<FStructuralNodeId> EchoProcessedPanelsInScope;
  TSet<FStructuralNodeId> EchoToggleHandledPanelsInScope;
  TSet<FStructuralNodeId> EchoProcessedSwitchesInScope;
  TSet<FStructuralNodeId> EchoToggleHandledSwitchesInScope;
  TMap<int32, FStructuralSiteFeedSignature> FeedSignatures;
};
