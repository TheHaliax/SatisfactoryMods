// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Config/FStructuralGroupToggleRegistry.h"

#include "Config/FStructuralPowerModConfig.h"
#include "Routing/EStructuralChannel.h"
#include "Save/AStructuralPowerGraphSubsystem.h"

FStructuralGroupToggleRegistry& FStructuralGroupToggleRegistry::Get() {
  static FStructuralGroupToggleRegistry Instance;
  return Instance;
}

void FStructuralGroupToggleRegistry::Initialize() {
  if (bInitialized) {
    return;
  }

  Definitions = {
      {
          TEXT("GroupLighting"),
          EStructuralChannel::Light,
          &FStructuralPowerModConfig::IsGroupLightingEnabled,
          &FStructuralPowerModConfig::RequestGroupLightingReconcile,
      },
      {
          TEXT("GroupGeneration"),
          EStructuralChannel::Generator,
          &FStructuralPowerModConfig::IsGroupGenerationEnabled,
          &FStructuralPowerModConfig::RequestGroupGenerationReconcile,
      },
      {
          TEXT("GroupResources"),
          EStructuralChannel::Extractor,
          &FStructuralPowerModConfig::IsGroupResourcesEnabled,
          &FStructuralPowerModConfig::RequestGroupResourcesReconcile,
      },
      {
          TEXT("GroupProduction"),
          EStructuralChannel::Manufacturer,
          &FStructuralPowerModConfig::IsGroupProductionEnabled,
          &FStructuralPowerModConfig::RequestGroupProductionReconcile,
      },
      {
          TEXT("GroupTransport"),
          EStructuralChannel::Transport,
          &FStructuralPowerModConfig::IsGroupTransportEnabled,
          &FStructuralPowerModConfig::RequestGroupTransportReconcile,
      },
      {
          TEXT("GroupPipes"),
          EStructuralChannel::Misc,
          &FStructuralPowerModConfig::IsGroupPipesEnabled,
          &FStructuralPowerModConfig::RequestGroupPipesReconcile,
      },
      {
          TEXT("GroupBelts"),
          EStructuralChannel::Misc,
          &FStructuralPowerModConfig::IsGroupBeltsEnabled,
          &FStructuralPowerModConfig::RequestGroupBeltsReconcile,
      },
  };

  bInitialized = true;
}

bool FStructuralGroupToggleRegistry::IsChannelRoutingEnabled(
    const EStructuralChannel Channel) const {
  for (const FStructuralGroupToggleDef& Def : Definitions) {
    if (Def.Channel == Channel && Def.IsEnabled) {
      return Def.IsEnabled();
    }
  }
  return true;
}

const FStructuralGroupToggleDef*
FStructuralGroupToggleRegistry::FindByKey(const FName ConfigKey) const {
  for (const FStructuralGroupToggleDef& Def : Definitions) {
    if (Def.ConfigKey == ConfigKey) {
      return &Def;
    }
  }
  return nullptr;
}

void FStructuralGroupToggleRegistry::ForEach(
    const TFunctionRef<void(const FStructuralGroupToggleDef&)> Visitor) const {
  for (const FStructuralGroupToggleDef& Def : Definitions) {
    Visitor(Def);
  }
}
