// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Core/EAttachContext.h"

bool IsBulkLoadAttachContext(const EAttachContext Context) {
  return Context == EAttachContext::BulkLoad;
}

bool ShouldSkipRuntimeRestitch(const EAttachContext Context) {
  return Context == EAttachContext::BulkLoad;
}

EAttachContext AttachContextFromBulkDrain(const bool bBulkLoadDrainActive) {
  return bBulkLoadDrainActive ? EAttachContext::BulkLoad : EAttachContext::RuntimePlace;
}

const TCHAR* AttachContextToString(const EAttachContext Context) {
  switch (Context) {
    case EAttachContext::BulkLoad:
      return TEXT("bulk");
    case EAttachContext::RuntimePlace:
      return TEXT("runtime");
    case EAttachContext::WireDelta:
      return TEXT("wire_delta");
    case EAttachContext::Toggle:
      return TEXT("toggle");
    default:
      return TEXT("?");
  }
}
