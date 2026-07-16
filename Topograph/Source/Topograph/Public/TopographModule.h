// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FTopographModule : public IModuleInterface {
 public:
  virtual void StartupModule() override;
  virtual void ShutdownModule() override;
};
