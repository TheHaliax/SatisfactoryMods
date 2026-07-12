// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Processors/FStructuralEndpointProcessors.h"

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
#include "Processors/FStructuralPowerPanelProcessor.h"
#include "Processors/FStructuralPowerPoleProcessor.h"
#include "Processors/FStructuralPowerProcessorRegistry.h"
#include "Processors/FStructuralPowerStorageProcessor.h"
#include "Processors/FStructuralPowerSwitchProcessor.h"
#include "Rules/FStructuralEligibilityRules.h"

namespace
{

bool GateGroupLighting()
{
	return FStructuralPowerModConfig::IsGroupLightingEnabled();
}

bool GateGroupGeneration()
{
	return FStructuralPowerModConfig::IsGroupGenerationEnabled();
}

FStructuralEndpointDescriptor MakeDescriptor(
	const EStructuralEndpointKind Kind,
	const EStructuralChannel Channel,
	const EStructuralPowerRole Role,
	const EStructuralAttachStrategy AttachStrategy,
	bool (*Classifier)(const AFGBuildable*),
	bool (*GroupGate)())
{
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

class FPoleEndpointProcessor final : public IStructuralEndpointProcessor
{
public:
	const FStructuralEndpointDescriptor& GetDescriptor() const override
	{
		static const FStructuralEndpointDescriptor Descriptor = MakeDescriptor(
			EStructuralEndpointKind::Pole,
			EStructuralChannel::Structure,
			EStructuralPowerRole::Router,
			EStructuralAttachStrategy::Bridge,
			&FStructuralEligibilityRules::IsPowerBridgePole,
			[]() { return true; });
		return Descriptor;
	}

	EStructuralEndpointKind GetBuildableKind() const override
	{
		return EStructuralEndpointKind::Pole;
	}

	EStructuralPowerRole GetPowerRole() const override
	{
		return EStructuralPowerRole::Router;
	}

	void ProcessPlacement(
		FStructuralPowerContext& Ctx,
		AFGBuildable* Buildable,
		const bool bLocalPromoteOnly) override
	{
		if (AFGBuildablePowerPole* Pole = FStructuralPowerBuildableCasts::AsPole(Buildable))
		{
			FStructuralPowerPoleProcessor::Process(Ctx, Pole);
		}
		(void)bLocalPromoteOnly;
	}

	void OnWireDelta(FStructuralPowerContext& Ctx, AFGBuildable* Buildable) override
	{
		if (AFGBuildablePowerPole* Pole = FStructuralPowerBuildableCasts::AsPole(Buildable))
		{
			FStructuralPowerPoleProcessor::OnWireDelta(Ctx, Pole);
		}
	}
};

class FStorageEndpointProcessor final : public IStructuralEndpointProcessor
{
public:
	const FStructuralEndpointDescriptor& GetDescriptor() const override
	{
		static const FStructuralEndpointDescriptor Descriptor = MakeDescriptor(
			EStructuralEndpointKind::Storage,
			EStructuralChannel::Generator,
			EStructuralPowerRole::Host,
			EStructuralAttachStrategy::Bridge,
			&FStructuralEligibilityRules::IsPowerStorage,
			&GateGroupGeneration);
		return Descriptor;
	}

	EStructuralEndpointKind GetBuildableKind() const override
	{
		return EStructuralEndpointKind::Storage;
	}

	EStructuralPowerRole GetPowerRole() const override
	{
		return EStructuralPowerRole::Host;
	}

	void ProcessPlacement(
		FStructuralPowerContext& Ctx,
		AFGBuildable* Buildable,
		const bool bLocalPromoteOnly) override
	{
		if (AFGBuildablePowerStorage* Storage = FStructuralPowerBuildableCasts::AsStorage(Buildable))
		{
			FStructuralPowerStorageProcessor::Process(Ctx, Storage);
		}
		(void)bLocalPromoteOnly;
	}

	void OnWireDelta(FStructuralPowerContext& Ctx, AFGBuildable* Buildable) override
	{
		if (AFGBuildablePowerStorage* Storage = FStructuralPowerBuildableCasts::AsStorage(Buildable))
		{
			FStructuralPowerStorageProcessor::OnWireDelta(Ctx, Storage);
		}
	}

	void TearDown(FStructuralPowerContext& Ctx, AFGBuildable* Buildable) override
	{
		if (AFGBuildablePowerStorage* Storage = FStructuralPowerBuildableCasts::AsStorage(Buildable))
		{
			FStructuralPowerStorageProcessor::TearDown(Ctx, Storage);
		}
	}

};

class FSwitchEndpointProcessor final : public IStructuralEndpointProcessor
{
public:
	const FStructuralEndpointDescriptor& GetDescriptor() const override
	{
		static const FStructuralEndpointDescriptor Descriptor = MakeDescriptor(
			EStructuralEndpointKind::Switch,
			EStructuralChannel::Switch,
			EStructuralPowerRole::Gateway,
			EStructuralAttachStrategy::ToggleBridge,
			&FStructuralEligibilityRules::IsPowerBridgeSwitch,
			[]() { return true; });
		return Descriptor;
	}

	EStructuralEndpointKind GetBuildableKind() const override
	{
		return EStructuralEndpointKind::Switch;
	}

	EStructuralPowerRole GetPowerRole() const override
	{
		return EStructuralPowerRole::Gateway;
	}

	void ProcessPlacement(
		FStructuralPowerContext& Ctx,
		AFGBuildable* Buildable,
		const bool bLocalPromoteOnly) override
	{
		if (AFGBuildableCircuitSwitch* Switch = FStructuralPowerBuildableCasts::AsSwitch(Buildable))
		{
			FStructuralPowerSwitchProcessor::Process(Ctx, Switch);
		}
		(void)bLocalPromoteOnly;
	}

	void OnWireDelta(FStructuralPowerContext& Ctx, AFGBuildable* Buildable) override
	{
		if (AFGBuildableCircuitSwitch* Switch = FStructuralPowerBuildableCasts::AsSwitch(Buildable))
		{
			FStructuralPowerSwitchProcessor::OnWireDelta(Ctx, Switch);
		}
	}

	void OnCircuitsRebuilt(FStructuralPowerContext& Ctx, AFGBuildable* Buildable) override
	{
		if (AFGBuildableCircuitSwitch* Switch = FStructuralPowerBuildableCasts::AsSwitch(Buildable))
		{
			FStructuralPowerSwitchProcessor::OnCircuitsRebuilt(Ctx, Switch);
			FStructuralPowerSwitchProcessor::EnsureListener(Ctx, Switch);
		}
	}

	void OnToggleChanged(FStructuralPowerContext& Ctx, AFGBuildable* Buildable) override
	{
		if (AFGBuildableCircuitSwitch* Switch = FStructuralPowerBuildableCasts::AsSwitch(Buildable))
		{
			FStructuralPowerSwitchProcessor::OnStateChanged(Ctx, Switch);
		}
	}

	void TearDown(FStructuralPowerContext& Ctx, AFGBuildable* Buildable) override
	{
		if (AFGBuildableCircuitSwitch* Switch = FStructuralPowerBuildableCasts::AsSwitch(Buildable))
		{
			FStructuralPowerSwitchProcessor::TearDown(Ctx, Switch);
		}
	}

};

class FLightEndpointProcessor final : public IStructuralEndpointProcessor
{
public:
	const FStructuralEndpointDescriptor& GetDescriptor() const override
	{
		static const FStructuralEndpointDescriptor Descriptor = MakeDescriptor(
			EStructuralEndpointKind::Light,
			EStructuralChannel::Light,
			EStructuralPowerRole::Host,
			EStructuralAttachStrategy::Consumer,
			&FStructuralEligibilityRules::IsStructuralLightConsumer,
			&GateGroupLighting);
		return Descriptor;
	}

	EStructuralEndpointKind GetBuildableKind() const override
	{
		return EStructuralEndpointKind::Light;
	}

	EStructuralPowerRole GetPowerRole() const override
	{
		return EStructuralPowerRole::Host;
	}

	void ProcessPlacement(
		FStructuralPowerContext& Ctx,
		AFGBuildable* Buildable,
		const bool bLocalPromoteOnly) override
	{
		if (AFGBuildableLightSource* Light = FStructuralPowerBuildableCasts::AsLight(Buildable))
		{
			FStructuralPowerLightProcessor::Process(Ctx, Light, bLocalPromoteOnly);
		}
	}

	void TearDown(FStructuralPowerContext& Ctx, AFGBuildable* Buildable) override
	{
		if (AFGBuildableLightSource* Light = FStructuralPowerBuildableCasts::AsLight(Buildable))
		{
			FStructuralPowerLightProcessor::TearDown(Ctx, Light);
		}
	}
};

class FPanelEndpointProcessor final : public IStructuralEndpointProcessor
{
public:
	const FStructuralEndpointDescriptor& GetDescriptor() const override
	{
		static const FStructuralEndpointDescriptor Descriptor = MakeDescriptor(
			EStructuralEndpointKind::Panel,
			EStructuralChannel::Light,
			EStructuralPowerRole::Router,
			EStructuralAttachStrategy::Router,
			[](const AFGBuildable* Buildable) { return IsValid(Buildable)
				&& Buildable->IsA<AFGBuildableLightsControlPanel>(); },
			&GateGroupLighting);
		return Descriptor;
	}

	EStructuralEndpointKind GetBuildableKind() const override
	{
		return EStructuralEndpointKind::Panel;
	}

	EStructuralPowerRole GetPowerRole() const override
	{
		return EStructuralPowerRole::Router;
	}

	void ProcessPlacement(
		FStructuralPowerContext& Ctx,
		AFGBuildable* Buildable,
		const bool bLocalPromoteOnly) override
	{
		if (AFGBuildableLightsControlPanel* Panel = FStructuralPowerBuildableCasts::AsPanel(Buildable))
		{
			FStructuralPowerPanelProcessor::Process(Ctx, Panel, bLocalPromoteOnly);
		}
	}

	void OnWireDelta(FStructuralPowerContext& Ctx, AFGBuildable* Buildable) override
	{
		if (AFGBuildableLightsControlPanel* Panel = FStructuralPowerBuildableCasts::AsPanel(Buildable))
		{
			FStructuralPowerPanelProcessor::OnWireDelta(Ctx, Panel);
		}
	}

	void TearDown(FStructuralPowerContext& Ctx, AFGBuildable* Buildable) override
	{
		if (AFGBuildableLightsControlPanel* Panel = FStructuralPowerBuildableCasts::AsPanel(Buildable))
		{
			FStructuralPowerPanelProcessor::TearDown(Ctx, Panel);
		}
	}

};

class FGeneratorEndpointProcessor final : public IStructuralEndpointProcessor
{
public:
	const FStructuralEndpointDescriptor& GetDescriptor() const override
	{
		static const FStructuralEndpointDescriptor Descriptor = MakeDescriptor(
			EStructuralEndpointKind::Generator,
			EStructuralChannel::Generator,
			EStructuralPowerRole::Host,
			EStructuralAttachStrategy::Consumer,
			&FStructuralEligibilityRules::IsStructuralGenerator,
			&GateGroupGeneration);
		return Descriptor;
	}

	EStructuralEndpointKind GetBuildableKind() const override
	{
		return EStructuralEndpointKind::Generator;
	}

	EStructuralPowerRole GetPowerRole() const override
	{
		return EStructuralPowerRole::Host;
	}

	void ProcessPlacement(
		FStructuralPowerContext& Ctx,
		AFGBuildable* Buildable,
		const bool bLocalPromoteOnly) override
	{
		if (AFGBuildableGenerator* Generator = Cast<AFGBuildableGenerator>(Buildable))
		{
			FStructuralPowerGeneratorProcessor::Process(Ctx, Generator);
		}
		(void)bLocalPromoteOnly;
	}

	void TearDown(FStructuralPowerContext& Ctx, AFGBuildable* Buildable) override
	{
		if (AFGBuildableGenerator* Generator = Cast<AFGBuildableGenerator>(Buildable))
		{
			FStructuralPowerGeneratorProcessor::TearDown(Ctx, Generator);
		}
	}
};

static FPoleEndpointProcessor GPoleProcessor;
static FStorageEndpointProcessor GStorageProcessor;
static FSwitchEndpointProcessor GSwitchProcessor;
static FLightEndpointProcessor GLightProcessor;
static FPanelEndpointProcessor GPanelProcessor;
static FGeneratorEndpointProcessor GGeneratorProcessor;

} // namespace

void FStructuralEndpointProcessors::RegisterAll(FStructuralEndpointCatalog& Catalog)
{
	Catalog.RegisterProcessor(GPoleProcessor);
	Catalog.RegisterProcessor(GStorageProcessor);
	Catalog.RegisterProcessor(GSwitchProcessor);
	Catalog.RegisterProcessor(GLightProcessor);
	Catalog.RegisterProcessor(GPanelProcessor);
	Catalog.RegisterProcessor(GGeneratorProcessor);
}

IStructuralEndpointProcessor& FStructuralEndpointProcessors::Pole()
{
	return GPoleProcessor;
}

IStructuralEndpointProcessor& FStructuralEndpointProcessors::Storage()
{
	return GStorageProcessor;
}

IStructuralEndpointProcessor& FStructuralEndpointProcessors::Switch()
{
	return GSwitchProcessor;
}

IStructuralEndpointProcessor& FStructuralEndpointProcessors::Light()
{
	return GLightProcessor;
}

IStructuralEndpointProcessor& FStructuralEndpointProcessors::Panel()
{
	return GPanelProcessor;
}

IStructuralEndpointProcessor& FStructuralEndpointProcessors::Generator()
{
	return GGeneratorProcessor;
}

void FStructuralEndpointProcessors::InitializeRegistries()
{
	FStructuralEndpointCatalog::Get().Initialize();
	FStructuralPowerProcessorRegistry::Get().Initialize();
}
