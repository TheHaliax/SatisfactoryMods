// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Core/EStructuralPowerRole.h"
#include "Core/EStructuralPowerScope.h"

const TCHAR* StructuralPowerScopeToString(const EStructuralPowerScope Scope) {
  switch (Scope) {
    case EStructuralPowerScope::Site:
      return TEXT("site");
    case EStructuralPowerScope::CrossSite:
      return TEXT("cross_site");
    default:
      return TEXT("?");
  }
}

const TCHAR* StructuralPowerRoleToString(const EStructuralPowerRole Role) {
  switch (Role) {
    case EStructuralPowerRole::Gateway:
      return TEXT("gateway");
    case EStructuralPowerRole::Router:
      return TEXT("router");
    case EStructuralPowerRole::Host:
      return TEXT("host");
    default:
      return TEXT("?");
  }
}
