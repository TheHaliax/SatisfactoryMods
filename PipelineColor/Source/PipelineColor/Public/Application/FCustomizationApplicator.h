// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "Appearance/FPCAppearanceSpec.h"

class AFGBuildable;

class FCustomizationApplicator {
 public:
  static bool ApplyIfChanged(AFGBuildable* Buildable, const FPCAppearanceSpec& Spec);
};
