// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Graph/FStructuralSiteState.h"

void FStructuralSiteState::BeginEchoScope(const TArray<int32>& SiteRoots) {
  ClearEchoScope();

  for (int32 SiteRoot : SiteRoots) {
    if (SiteRoot != INDEX_NONE) {
      EchoDirtySites.Add(SiteRoot);
    }
  }
}

void FStructuralSiteState::ClearEchoScope() {
  EchoDirtySites.Reset();
  EchoProcessedPanelsInScope.Reset();
  EchoToggleHandledPanelsInScope.Reset();
  EchoProcessedSwitchesInScope.Reset();
  EchoToggleHandledSwitchesInScope.Reset();
}

bool FStructuralSiteState::IsEchoScopeActive() const {
  return EchoDirtySites.Num() > 0;
}

bool FStructuralSiteState::IsEchoDirtySite(int32 SiteRoot) const {
  return SiteRoot != INDEX_NONE && EchoDirtySites.Contains(SiteRoot);
}

bool FStructuralSiteState::WasPanelEchoProcessedInScope(const FStructuralNodeId& PanelId) const {
  return EchoProcessedPanelsInScope.Contains(PanelId);
}

void FStructuralSiteState::NotePanelEchoProcessed(const FStructuralNodeId& PanelId) {
  EchoProcessedPanelsInScope.Add(PanelId);
}

bool FStructuralSiteState::WasPanelToggleHandledInScope(const FStructuralNodeId& PanelId) const {
  return EchoToggleHandledPanelsInScope.Contains(PanelId);
}

void FStructuralSiteState::NotePanelToggleHandled(const FStructuralNodeId& PanelId) {
  EchoToggleHandledPanelsInScope.Add(PanelId);
}

bool FStructuralSiteState::WasSwitchEchoProcessedInScope(const FStructuralNodeId& SwitchId) const {
  return EchoProcessedSwitchesInScope.Contains(SwitchId);
}

void FStructuralSiteState::NoteSwitchEchoProcessed(const FStructuralNodeId& SwitchId) {
  EchoProcessedSwitchesInScope.Add(SwitchId);
}

bool FStructuralSiteState::WasSwitchToggleHandledInScope(const FStructuralNodeId& SwitchId) const {
  return EchoToggleHandledSwitchesInScope.Contains(SwitchId);
}

void FStructuralSiteState::NoteSwitchToggleHandled(const FStructuralNodeId& SwitchId) {
  EchoToggleHandledSwitchesInScope.Add(SwitchId);
}

void FStructuralSiteState::ClearFeedSignatures() {
  FeedSignatures.Reset();
}

void FStructuralSiteState::SeedFeedSignature(int32 Site,
                                             const FStructuralSiteFeedSignature& Signature) {
  if (Site != INDEX_NONE) {
    FeedSignatures.Add(Site, Signature);
  }
}

bool FStructuralSiteState::TryGetFeedSignature(int32 Site,
                                               FStructuralSiteFeedSignature& OutSignature) const {
  if (Site == INDEX_NONE) {
    return false;
  }

  if (const FStructuralSiteFeedSignature* Cached = FeedSignatures.Find(Site)) {
    OutSignature = *Cached;
    return true;
  }

  return false;
}

void FStructuralSiteState::SetFeedSignature(int32 Site,
                                            const FStructuralSiteFeedSignature& Signature) {
  if (Site != INDEX_NONE) {
    FeedSignatures.Add(Site, Signature);
  }
}
