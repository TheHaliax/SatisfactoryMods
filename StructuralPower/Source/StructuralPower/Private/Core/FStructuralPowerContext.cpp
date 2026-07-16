// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Core/FStructuralPowerContext.h"

#include "Core/FStructuralGraphSession.h"
#include "Engine/World.h"

FStructuralPowerContext::FStructuralPowerContext(FStructuralGraphSession& InSession,
                                                 const EAttachContext InAttachContext,
                                                 const int32 InSiteRoot)
    : SessionPtr(&InSession), AttachContext(InAttachContext), SiteRoot(InSiteRoot) {
}

FStructuralGraphSession& FStructuralPowerContext::Session() {
  check(SessionPtr);
  return *SessionPtr;
}

const FStructuralGraphSession& FStructuralPowerContext::Session() const {
  check(SessionPtr);
  return *SessionPtr;
}

int32 FStructuralPowerContext::ResolveSiteRoot(const int32 FallbackRoot) const {
  return SiteRoot != INDEX_NONE ? SiteRoot : FallbackRoot;
}

UWorld* FStructuralPowerContext::GetWorld() const {
  return SessionPtr ? SessionPtr->GetWorld() : nullptr;
}
