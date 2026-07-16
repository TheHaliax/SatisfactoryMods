// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "Appearance/FPCAppearanceSpec.h"
#include "CoreMinimal.h"
#include "Resources/FGItemDescriptor.h"
#include "Templates/SubclassOf.h"

class IAppearanceCatalog {
 public:
  virtual ~IAppearanceCatalog() = default;

  virtual bool Resolve(TSubclassOf<UFGItemDescriptor> FluidDescriptor, bool bEmpty,
                       FPCAppearanceSpec& OutSpec) const = 0;
};
