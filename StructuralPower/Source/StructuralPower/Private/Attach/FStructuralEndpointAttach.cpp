// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Attach/FStructuralEndpointAttach.h"

#include "Attach/FStructuralBridgeAttach.h"
#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Buildables/FGBuildableGenerator.h"
#include "Buildables/FGBuildableLightSource.h"
#include "Buildables/FGBuildableLightsControlPanel.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "Buildables/FGBuildablePowerStorage.h"
#include "Config/FStructuralPowerModConfig.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Graph/FStructuralPowerBuildableCasts.h"
#include "Processors/FStructuralPowerGeneratorProcessor.h"
#include "Processors/FStructuralPowerLightProcessor.h"
#include "Processors/FStructuralPowerMachineConsumerProcessor.h"
#include "Processors/FStructuralPowerPanelProcessor.h"
#include "Processors/FStructuralPowerPoleProcessor.h"
#include "Processors/FStructuralPowerStorageProcessor.h"
#include "Processors/FStructuralPowerSwitchProcessor.h"
#include "Rules/FStructuralEligibilityRules.h"

FStructuralBridgeAttachOutcome FStructuralEndpointAttach::AttachOnPlace(
    FStructuralPowerContext& Ctx, const FStructuralBridgeAttachRequest& Request) {
  return FStructuralBridgeAttach::AttachOnPlace(Ctx, Request);
}

bool FStructuralEndpointAttach::AttachConsumer(FStructuralPowerContext& Ctx, AFGBuildable* Host,
                                               const bool bLocalPromoteOnly) {
  if (AFGBuildableLightSource* Light = FStructuralPowerBuildableCasts::AsLight(Host)) {
    FStructuralPowerLightProcessor::Process(Ctx, Light, bLocalPromoteOnly);
    return true;
  }

  if (FStructuralEligibilityRules::IsStructuralGenerator(Host)) {
    if (AFGBuildableGenerator* Generator = Cast<AFGBuildableGenerator>(Host)) {
      FStructuralPowerGeneratorProcessor::Process(Ctx, Generator);
    } else {
      FStructuralPowerGeneratorProcessor::ProcessFactoryHost(Ctx, Host);
    }
    return true;
  }

  if (FStructuralEligibilityRules::IsStructuralExtractor(Host)) {
    FStructuralPowerMachineConsumerProcessor::Process(
        Ctx, Host, EStructuralEndpointKind::Extractor,
        &FStructuralPowerModConfig::IsGroupResourcesEnabled);
    return true;
  }

  if (FStructuralEligibilityRules::IsStructuralManufacturer(Host)) {
    FStructuralPowerMachineConsumerProcessor::Process(
        Ctx, Host, EStructuralEndpointKind::Manufacturer,
        &FStructuralPowerModConfig::IsGroupProductionEnabled);
    return true;
  }

  if (FStructuralEligibilityRules::IsStructuralTransport(Host)) {
    FStructuralPowerMachineConsumerProcessor::Process(
        Ctx, Host, EStructuralEndpointKind::Transport,
        &FStructuralPowerModConfig::IsGroupTransportEnabled);
    return true;
  }

  if (FStructuralEligibilityRules::IsStructuralPipelinePump(Host)) {
    FStructuralPowerMachineConsumerProcessor::Process(
        Ctx, Host, EStructuralEndpointKind::PipePump,
        &FStructuralPowerModConfig::IsGroupPipesEnabled);
    return true;
  }

  return false;
}

bool FStructuralEndpointAttach::AttachRouter(FStructuralPowerContext& Ctx, AFGBuildable* Host,
                                             const bool bLocalPromoteOnly) {
  if (AFGBuildableLightsControlPanel* Panel = FStructuralPowerBuildableCasts::AsPanel(Host)) {
    FStructuralPowerPanelProcessor::Process(Ctx, Panel, bLocalPromoteOnly);
    return true;
  }
  return false;
}

bool FStructuralEndpointAttach::AttachToggleBridge(FStructuralPowerContext& Ctx,
                                                   AFGBuildable* Host) {
  if (AFGBuildableCircuitSwitch* Switch = FStructuralPowerBuildableCasts::AsSwitch(Host)) {
    FStructuralPowerSwitchProcessor::Process(Ctx, Switch);
    return true;
  }
  return false;
}

bool FStructuralEndpointAttach::RunStrategy(FStructuralPowerContext& Ctx, AFGBuildable* Host,
                                            const EStructuralAttachStrategy Strategy,
                                            const bool bLocalPromoteOnly) {
  switch (Strategy) {
    case EStructuralAttachStrategy::Bridge: {
      if (AFGBuildablePowerPole* Pole = FStructuralPowerBuildableCasts::AsPole(Host)) {
        FStructuralPowerPoleProcessor::Process(Ctx, Pole);
        return true;
      }
      if (AFGBuildablePowerStorage* Storage = FStructuralPowerBuildableCasts::AsStorage(Host)) {
        FStructuralPowerStorageProcessor::Process(Ctx, Storage);
        return true;
      }
      return false;
    }
    case EStructuralAttachStrategy::ToggleBridge:
      return AttachToggleBridge(Ctx, Host);
    case EStructuralAttachStrategy::Consumer:
      return AttachConsumer(Ctx, Host, bLocalPromoteOnly);
    case EStructuralAttachStrategy::Router:
      return AttachRouter(Ctx, Host, bLocalPromoteOnly);
    default:
      return false;
  }
}
