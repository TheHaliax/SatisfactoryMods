// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Processors/FStructuralEndpointProcessors.h"

#include "Attach/FStructuralEndpointAttach.h"
#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Buildables/FGBuildableGenerator.h"
#include "Buildables/FGBuildableLightSource.h"
#include "Buildables/FGBuildableLightsControlPanel.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "Buildables/FGBuildablePowerStorage.h"
#include "Config/FStructuralPowerModConfig.h"
#include "Core/FStructuralGraphSession.h"
#include "Graph/FStructuralPowerBuildableCasts.h"
#include "Processors/FStructuralEndpointCatalog.h"
#include "Processors/FStructuralPowerGeneratorProcessor.h"
#include "Processors/FStructuralPowerLightProcessor.h"
#include "Processors/FStructuralPowerMachineConsumerProcessor.h"
#include "Processors/FStructuralPowerPanelProcessor.h"
#include "Processors/FStructuralPowerPoleProcessor.h"
#include "Processors/FStructuralPowerStorageProcessor.h"
#include "Processors/FStructuralPowerSwitchProcessor.h"
#include "Rules/FStructuralEligibilityRules.h"

namespace {

bool GateGroupLighting() {
  return FStructuralPowerModConfig::IsGroupLightingEnabled();
}

bool GateGroupGeneration() {
  return FStructuralPowerModConfig::IsGroupGenerationEnabled();
}

bool GateGroupResources() {
  return FStructuralPowerModConfig::IsGroupResourcesEnabled();
}

bool GateGroupProduction() {
  return FStructuralPowerModConfig::IsGroupProductionEnabled();
}

bool GateGroupTransport() {
  return FStructuralPowerModConfig::IsGroupTransportEnabled();
}

bool GateGroupPipes() {
  return FStructuralPowerModConfig::IsGroupPipesEnabled();
}

FStructuralEndpointDescriptor MakeDescriptor(const EStructuralEndpointKind Kind,
                                             const EStructuralChannel Channel,
                                             const EStructuralPowerRole Role,
                                             const EStructuralAttachStrategy AttachStrategy,
                                             bool (*Classifier)(const AFGBuildable*),
                                             bool (*GroupGate)()) {
  FStructuralEndpointDescriptor Descriptor;
  Descriptor.Kind = Kind;
  Descriptor.Channel = Channel;
  Descriptor.Role = Role;
  Descriptor.AttachStrategy = AttachStrategy;
  Descriptor.PlacementJob = EStructuralPlacementJobType::Outlet;
  Descriptor.Classifier = Classifier;
  Descriptor.GroupGate = GroupGate;
  return Descriptor;
}

class FPoleEndpointProcessor final : public IStructuralEndpointProcessor {
 public:
  const FStructuralEndpointDescriptor& GetDescriptor() const override {
    static const FStructuralEndpointDescriptor Descriptor =
        MakeDescriptor(EStructuralEndpointKind::Pole, EStructuralChannel::Structure,
                       EStructuralPowerRole::Router, EStructuralAttachStrategy::Bridge,
                       &FStructuralEligibilityRules::IsPowerBridgePole, []() { return true; });
    return Descriptor;
  }

  EStructuralEndpointKind GetBuildableKind() const override {
    return EStructuralEndpointKind::Pole;
  }

  EStructuralPowerRole GetPowerRole() const override {
    return EStructuralPowerRole::Router;
  }

  void ProcessPlacement(FStructuralPowerContext& Ctx, AFGBuildable* Buildable,
                        const bool bLocalPromoteOnly) override {
    FStructuralEndpointAttach::RunStrategy(Ctx, Buildable, GetDescriptor().AttachStrategy,
                                           bLocalPromoteOnly);
  }

  void OnWireDelta(FStructuralPowerContext& Ctx, AFGBuildable* Buildable) override {
    ProcessPlacement(Ctx, Buildable, /*bLocalPromoteOnly=*/false);
  }
};

class FStorageEndpointProcessor final : public IStructuralEndpointProcessor {
 public:
  const FStructuralEndpointDescriptor& GetDescriptor() const override {
    static const FStructuralEndpointDescriptor Descriptor =
        MakeDescriptor(EStructuralEndpointKind::Storage, EStructuralChannel::Generator,
                       EStructuralPowerRole::Host, EStructuralAttachStrategy::Bridge,
                       &FStructuralEligibilityRules::IsPowerStorage, &GateGroupGeneration);
    return Descriptor;
  }

  EStructuralEndpointKind GetBuildableKind() const override {
    return EStructuralEndpointKind::Storage;
  }

  EStructuralPowerRole GetPowerRole() const override {
    return EStructuralPowerRole::Host;
  }

  void ProcessPlacement(FStructuralPowerContext& Ctx, AFGBuildable* Buildable,
                        const bool bLocalPromoteOnly) override {
    FStructuralEndpointAttach::RunStrategy(Ctx, Buildable, GetDescriptor().AttachStrategy,
                                           bLocalPromoteOnly);
  }

  void OnWireDelta(FStructuralPowerContext& Ctx, AFGBuildable* Buildable) override {
    ProcessPlacement(Ctx, Buildable, /*bLocalPromoteOnly=*/false);
  }

  void TearDown(FStructuralPowerContext& Ctx, AFGBuildable* Buildable) override {
    if (AFGBuildablePowerStorage* Storage = FStructuralPowerBuildableCasts::AsStorage(Buildable)) {
      FStructuralPowerStorageProcessor::TearDown(Ctx, Storage);
    }
  }
};

class FSwitchEndpointProcessor final : public IStructuralEndpointProcessor {
 public:
  const FStructuralEndpointDescriptor& GetDescriptor() const override {
    static const FStructuralEndpointDescriptor Descriptor =
        MakeDescriptor(EStructuralEndpointKind::Switch, EStructuralChannel::Switch,
                       EStructuralPowerRole::Gateway, EStructuralAttachStrategy::ToggleBridge,
                       &FStructuralEligibilityRules::IsPowerBridgeSwitch, []() { return true; });
    return Descriptor;
  }

  EStructuralEndpointKind GetBuildableKind() const override {
    return EStructuralEndpointKind::Switch;
  }

  EStructuralPowerRole GetPowerRole() const override {
    return EStructuralPowerRole::Gateway;
  }

  void ProcessPlacement(FStructuralPowerContext& Ctx, AFGBuildable* Buildable,
                        const bool bLocalPromoteOnly) override {
    FStructuralEndpointAttach::RunStrategy(Ctx, Buildable, GetDescriptor().AttachStrategy,
                                           bLocalPromoteOnly);
  }

  void OnWireDelta(FStructuralPowerContext& Ctx, AFGBuildable* Buildable) override {
    ProcessPlacement(Ctx, Buildable, /*bLocalPromoteOnly=*/false);
  }

  void OnCircuitsRebuilt(FStructuralPowerContext& Ctx, AFGBuildable* Buildable) override {
    if (AFGBuildableCircuitSwitch* Switch = FStructuralPowerBuildableCasts::AsSwitch(Buildable)) {
      FStructuralPowerSwitchProcessor::OnCircuitsRebuilt(Ctx, Switch);
      FStructuralPowerSwitchProcessor::EnsureListener(Ctx, Switch);
    }
  }

  void OnToggleChanged(FStructuralPowerContext& Ctx, AFGBuildable* Buildable) override {
    if (AFGBuildableCircuitSwitch* Switch = FStructuralPowerBuildableCasts::AsSwitch(Buildable)) {
      FStructuralPowerSwitchProcessor::OnStateChanged(Ctx, Switch);
    }
  }

  void TearDown(FStructuralPowerContext& Ctx, AFGBuildable* Buildable) override {
    if (AFGBuildableCircuitSwitch* Switch = FStructuralPowerBuildableCasts::AsSwitch(Buildable)) {
      FStructuralPowerSwitchProcessor::TearDown(Ctx, Switch);
    }
  }
};

class FLightEndpointProcessor final : public IStructuralEndpointProcessor {
 public:
  const FStructuralEndpointDescriptor& GetDescriptor() const override {
    static const FStructuralEndpointDescriptor Descriptor =
        MakeDescriptor(EStructuralEndpointKind::Light, EStructuralChannel::Light,
                       EStructuralPowerRole::Host, EStructuralAttachStrategy::Consumer,
                       &FStructuralEligibilityRules::IsStructuralLightConsumer, &GateGroupLighting);
    return Descriptor;
  }

  EStructuralEndpointKind GetBuildableKind() const override {
    return EStructuralEndpointKind::Light;
  }

  EStructuralPowerRole GetPowerRole() const override {
    return EStructuralPowerRole::Host;
  }

  void ProcessPlacement(FStructuralPowerContext& Ctx, AFGBuildable* Buildable,
                        const bool bLocalPromoteOnly) override {
    FStructuralEndpointAttach::RunStrategy(Ctx, Buildable, GetDescriptor().AttachStrategy,
                                           bLocalPromoteOnly);
  }

  void TearDown(FStructuralPowerContext& Ctx, AFGBuildable* Buildable) override {
    if (AFGBuildableLightSource* Light = FStructuralPowerBuildableCasts::AsLight(Buildable)) {
      FStructuralPowerLightProcessor::TearDown(Ctx, Light);
    }
  }
};

class FPanelEndpointProcessor final : public IStructuralEndpointProcessor {
 public:
  const FStructuralEndpointDescriptor& GetDescriptor() const override {
    static const FStructuralEndpointDescriptor Descriptor = MakeDescriptor(
        EStructuralEndpointKind::Panel, EStructuralChannel::Light, EStructuralPowerRole::Router,
        EStructuralAttachStrategy::Router,
        [](const AFGBuildable* Buildable) {
          return IsValid(Buildable) && Buildable->IsA<AFGBuildableLightsControlPanel>();
        },
        &GateGroupLighting);
    return Descriptor;
  }

  EStructuralEndpointKind GetBuildableKind() const override {
    return EStructuralEndpointKind::Panel;
  }

  EStructuralPowerRole GetPowerRole() const override {
    return EStructuralPowerRole::Router;
  }

  void ProcessPlacement(FStructuralPowerContext& Ctx, AFGBuildable* Buildable,
                        const bool bLocalPromoteOnly) override {
    FStructuralEndpointAttach::RunStrategy(Ctx, Buildable, GetDescriptor().AttachStrategy,
                                           bLocalPromoteOnly);
  }

  void OnWireDelta(FStructuralPowerContext& Ctx, AFGBuildable* Buildable) override {
    ProcessPlacement(Ctx, Buildable, /*bLocalPromoteOnly=*/true);
  }

  void TearDown(FStructuralPowerContext& Ctx, AFGBuildable* Buildable) override {
    if (AFGBuildableLightsControlPanel* Panel =
            FStructuralPowerBuildableCasts::AsPanel(Buildable)) {
      FStructuralPowerPanelProcessor::TearDown(Ctx, Panel);
    }
  }
};

class FGeneratorEndpointProcessor final : public IStructuralEndpointProcessor {
 public:
  const FStructuralEndpointDescriptor& GetDescriptor() const override {
    static const FStructuralEndpointDescriptor Descriptor =
        MakeDescriptor(EStructuralEndpointKind::Generator, EStructuralChannel::Generator,
                       EStructuralPowerRole::Host, EStructuralAttachStrategy::Consumer,
                       &FStructuralEligibilityRules::IsStructuralGenerator, &GateGroupGeneration);
    return Descriptor;
  }

  EStructuralEndpointKind GetBuildableKind() const override {
    return EStructuralEndpointKind::Generator;
  }

  EStructuralPowerRole GetPowerRole() const override {
    return EStructuralPowerRole::Host;
  }

  void ProcessPlacement(FStructuralPowerContext& Ctx, AFGBuildable* Buildable,
                        const bool bLocalPromoteOnly) override {
    FStructuralEndpointAttach::RunStrategy(Ctx, Buildable, GetDescriptor().AttachStrategy,
                                           bLocalPromoteOnly);
  }

  void OnWireDelta(FStructuralPowerContext& Ctx, AFGBuildable* Buildable) override {
    ProcessPlacement(Ctx, Buildable, /*bLocalPromoteOnly=*/false);
  }

  void TearDown(FStructuralPowerContext& Ctx, AFGBuildable* Buildable) override {
    if (AFGBuildableGenerator* Generator = Cast<AFGBuildableGenerator>(Buildable)) {
      FStructuralPowerGeneratorProcessor::TearDown(Ctx, Generator);
    } else {
      FStructuralPowerGeneratorProcessor::TearDownFactoryHost(Ctx, Buildable);
    }
  }
};

class FExtractorEndpointProcessor final : public IStructuralEndpointProcessor {
 public:
  const FStructuralEndpointDescriptor& GetDescriptor() const override {
    static const FStructuralEndpointDescriptor Descriptor =
        MakeDescriptor(EStructuralEndpointKind::Extractor, EStructuralChannel::Extractor,
                       EStructuralPowerRole::Host, EStructuralAttachStrategy::Consumer,
                       &FStructuralEligibilityRules::IsStructuralExtractor, &GateGroupResources);
    return Descriptor;
  }

  EStructuralEndpointKind GetBuildableKind() const override {
    return EStructuralEndpointKind::Extractor;
  }

  EStructuralPowerRole GetPowerRole() const override {
    return EStructuralPowerRole::Host;
  }

  void ProcessPlacement(FStructuralPowerContext& Ctx, AFGBuildable* Buildable,
                        const bool bLocalPromoteOnly) override {
    FStructuralEndpointAttach::RunStrategy(Ctx, Buildable, GetDescriptor().AttachStrategy,
                                           bLocalPromoteOnly);
  }

  void OnWireDelta(FStructuralPowerContext& Ctx, AFGBuildable* Buildable) override {
    ProcessPlacement(Ctx, Buildable, /*bLocalPromoteOnly=*/false);
  }

  void TearDown(FStructuralPowerContext& Ctx, AFGBuildable* Buildable) override {
    FStructuralPowerMachineConsumerProcessor::TearDown(Ctx, Buildable);
  }
};

class FManufacturerEndpointProcessor final : public IStructuralEndpointProcessor {
 public:
  const FStructuralEndpointDescriptor& GetDescriptor() const override {
    static const FStructuralEndpointDescriptor Descriptor = MakeDescriptor(
        EStructuralEndpointKind::Manufacturer, EStructuralChannel::Manufacturer,
        EStructuralPowerRole::Host, EStructuralAttachStrategy::Consumer,
        &FStructuralEligibilityRules::IsStructuralManufacturer, &GateGroupProduction);
    return Descriptor;
  }

  EStructuralEndpointKind GetBuildableKind() const override {
    return EStructuralEndpointKind::Manufacturer;
  }

  EStructuralPowerRole GetPowerRole() const override {
    return EStructuralPowerRole::Host;
  }

  void ProcessPlacement(FStructuralPowerContext& Ctx, AFGBuildable* Buildable,
                        const bool bLocalPromoteOnly) override {
    FStructuralEndpointAttach::RunStrategy(Ctx, Buildable, GetDescriptor().AttachStrategy,
                                           bLocalPromoteOnly);
  }

  void OnWireDelta(FStructuralPowerContext& Ctx, AFGBuildable* Buildable) override {
    ProcessPlacement(Ctx, Buildable, /*bLocalPromoteOnly=*/false);
  }

  void TearDown(FStructuralPowerContext& Ctx, AFGBuildable* Buildable) override {
    FStructuralPowerMachineConsumerProcessor::TearDown(Ctx, Buildable);
  }
};

class FTransportEndpointProcessor final : public IStructuralEndpointProcessor {
 public:
  const FStructuralEndpointDescriptor& GetDescriptor() const override {
    static const FStructuralEndpointDescriptor Descriptor =
        MakeDescriptor(EStructuralEndpointKind::Transport, EStructuralChannel::Transport,
                       EStructuralPowerRole::Host, EStructuralAttachStrategy::Consumer,
                       &FStructuralEligibilityRules::IsStructuralTransport, &GateGroupTransport);
    return Descriptor;
  }

  EStructuralEndpointKind GetBuildableKind() const override {
    return EStructuralEndpointKind::Transport;
  }

  EStructuralPowerRole GetPowerRole() const override {
    return EStructuralPowerRole::Host;
  }

  void ProcessPlacement(FStructuralPowerContext& Ctx, AFGBuildable* Buildable,
                        const bool bLocalPromoteOnly) override {
    FStructuralEndpointAttach::RunStrategy(Ctx, Buildable, GetDescriptor().AttachStrategy,
                                           bLocalPromoteOnly);
  }

  void OnWireDelta(FStructuralPowerContext& Ctx, AFGBuildable* Buildable) override {
    ProcessPlacement(Ctx, Buildable, /*bLocalPromoteOnly=*/false);
  }

  void TearDown(FStructuralPowerContext& Ctx, AFGBuildable* Buildable) override {
    FStructuralPowerMachineConsumerProcessor::TearDown(Ctx, Buildable);
  }
};

class FPipePumpEndpointProcessor final : public IStructuralEndpointProcessor {
 public:
  const FStructuralEndpointDescriptor& GetDescriptor() const override {
    static const FStructuralEndpointDescriptor Descriptor =
        MakeDescriptor(EStructuralEndpointKind::PipePump, EStructuralChannel::Misc,
                       EStructuralPowerRole::Host, EStructuralAttachStrategy::Consumer,
                       &FStructuralEligibilityRules::IsStructuralPipelinePump, &GateGroupPipes);
    return Descriptor;
  }

  EStructuralEndpointKind GetBuildableKind() const override {
    return EStructuralEndpointKind::PipePump;
  }

  EStructuralPowerRole GetPowerRole() const override {
    return EStructuralPowerRole::Host;
  }

  void ProcessPlacement(FStructuralPowerContext& Ctx, AFGBuildable* Buildable,
                        const bool bLocalPromoteOnly) override {
    FStructuralEndpointAttach::RunStrategy(Ctx, Buildable, GetDescriptor().AttachStrategy,
                                           bLocalPromoteOnly);
  }

  void OnWireDelta(FStructuralPowerContext& Ctx, AFGBuildable* Buildable) override {
    ProcessPlacement(Ctx, Buildable, /*bLocalPromoteOnly=*/false);
  }

  void TearDown(FStructuralPowerContext& Ctx, AFGBuildable* Buildable) override {
    FStructuralPowerMachineConsumerProcessor::TearDown(Ctx, Buildable);
  }
};

static FPoleEndpointProcessor GPoleProcessor;
static FStorageEndpointProcessor GStorageProcessor;
static FSwitchEndpointProcessor GSwitchProcessor;
static FLightEndpointProcessor GLightProcessor;
static FPanelEndpointProcessor GPanelProcessor;
static FGeneratorEndpointProcessor GGeneratorProcessor;
static FExtractorEndpointProcessor GExtractorProcessor;
static FManufacturerEndpointProcessor GManufacturerProcessor;
static FTransportEndpointProcessor GTransportProcessor;
static FPipePumpEndpointProcessor GPipePumpProcessor;

}  // namespace

void FStructuralEndpointProcessors::RegisterAll(FStructuralEndpointCatalog& Catalog) {
  Catalog.RegisterProcessor(GPoleProcessor);
  Catalog.RegisterProcessor(GStorageProcessor);
  Catalog.RegisterProcessor(GSwitchProcessor);
  Catalog.RegisterProcessor(GLightProcessor);
  Catalog.RegisterProcessor(GPanelProcessor);
  Catalog.RegisterProcessor(GGeneratorProcessor);
  Catalog.RegisterProcessor(GExtractorProcessor);
  Catalog.RegisterProcessor(GManufacturerProcessor);
  Catalog.RegisterProcessor(GTransportProcessor);
  Catalog.RegisterProcessor(GPipePumpProcessor);
}

IStructuralEndpointProcessor& FStructuralEndpointProcessors::Pole() {
  return GPoleProcessor;
}

IStructuralEndpointProcessor& FStructuralEndpointProcessors::Storage() {
  return GStorageProcessor;
}

IStructuralEndpointProcessor& FStructuralEndpointProcessors::Switch() {
  return GSwitchProcessor;
}

IStructuralEndpointProcessor& FStructuralEndpointProcessors::Light() {
  return GLightProcessor;
}

IStructuralEndpointProcessor& FStructuralEndpointProcessors::Panel() {
  return GPanelProcessor;
}

IStructuralEndpointProcessor& FStructuralEndpointProcessors::Generator() {
  return GGeneratorProcessor;
}

void FStructuralEndpointProcessors::InitializeRegistries() {
  FStructuralEndpointCatalog::Get().Initialize();
}
