// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "Content/IContentKeyResolver.h"
#include "CoreMinimal.h"

class FPipeFluidKeyResolver final : public IContentKeyResolver {
 public:
  virtual bool Supports(AFGBuildable* Buildable) const override;
  virtual void Resolve(AFGBuildable* Buildable, TSubclassOf<UFGItemDescriptor>& OutFluid,
                       bool& bOutEmpty) const override;

  static bool IsPipeColorTarget(AFGBuildable* Buildable);
};
