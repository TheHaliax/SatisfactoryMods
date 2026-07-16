// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Target/FBuildableColorTarget.h"

#include "Application/FCustomizationApplicator.h"
#include "Buildables/FGBuildable.h"

FBuildableColorTarget::FBuildableColorTarget(AFGBuildable* InBuildable) : Buildable(InBuildable) {
}

bool FBuildableColorTarget::IsValid() const {
  return ::IsValid(Buildable) && Buildable->HasAuthority();
}

bool FBuildableColorTarget::Apply(const FPCAppearanceSpec& Spec) const {
  if (!IsValid()) {
    return false;
  }
  return FCustomizationApplicator::ApplyIfChanged(Buildable, Spec);
}
