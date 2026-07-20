// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "Appearance/FPCAppearanceSpec.h"

class AFGBuildable;

class FBuildableColorTarget {
 public:
  explicit FBuildableColorTarget(AFGBuildable* InBuildable);

  bool IsValid() const;
  AFGBuildable* Get() const {
    return Buildable;
  }
  bool Apply(const FPCAppearanceSpec& Spec) const;

 private:
  AFGBuildable* Buildable = nullptr;
};
