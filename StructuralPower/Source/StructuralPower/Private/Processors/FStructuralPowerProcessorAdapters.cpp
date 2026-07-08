// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Processors/FStructuralPowerProcessorRegistry.h"

#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Buildables/FGBuildableLightSource.h"
#include "Buildables/FGBuildableLightsControlPanel.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "Buildables/FGBuildablePowerStorage.h"
#include "Connection/FStructuralPoleConnectionPoint.h"
#include "Connection/FStructuralPanelConnectionPoint.h"
#include "Connection/FStructuralStorageConnectionPoint.h"
#include "Connection/FStructuralSwitchConnectionPoint.h"
#include "Core/EAttachContext.h"
#include "Core/FStructuralPowerContext.h"
#include "Graph/FStructuralPowerBuildableCasts.h"
#include "Processors/FStructuralPowerLightProcessor.h"
#include "Processors/FStructuralPowerPanelProcessor.h"
#include "Processors/FStructuralPowerPoleProcessor.h"
#include "Processors/FStructuralPowerStorageProcessor.h"
#include "Processors/FStructuralPowerSwitchProcessor.h"
#include "Processors/IStructuralPowerProcessor.h"
#include "Save/AStructuralPowerGraphSubsystem.h"

namespace
{

class FStructuralPowerLightProcessorAdapter final : public IStructuralPowerProcessor
{
public:
	EStructuralEndpointKind GetBuildableKind() const override
	{
		return EStructuralEndpointKind::Light;
	}

	EStructuralPowerRole GetPowerRole() const override
	{
		return EStructuralPowerRole::Host;
	}

	void Process(
		FStructuralPowerContext& Ctx,
		AFGBuildable* Buildable,
		bool bLocalPromoteOnly) override
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

class FStructuralPowerPanelProcessorAdapter final : public IStructuralPowerProcessor
{
public:
	EStructuralEndpointKind GetBuildableKind() const override
	{
		return EStructuralEndpointKind::Panel;
	}

	EStructuralPowerRole GetPowerRole() const override
	{
		return EStructuralPowerRole::Router;
	}

	void Process(
		FStructuralPowerContext& Ctx,
		AFGBuildable* Buildable,
		bool bLocalPromoteOnly) override
	{
		if (AFGBuildableLightsControlPanel* Panel = FStructuralPowerBuildableCasts::AsPanel(Buildable))
		{
			FStructuralPowerPanelProcessor::Process(Ctx, Panel, bLocalPromoteOnly);
		}
	}

	void TearDown(FStructuralPowerContext& Ctx, AFGBuildable* Buildable) override
	{
		if (AFGBuildableLightsControlPanel* Panel = FStructuralPowerBuildableCasts::AsPanel(Buildable))
		{
			FStructuralPowerPanelProcessor::TearDown(Ctx, Panel);
		}
	}

	void OnWireDelta(FStructuralPowerContext& Ctx, AFGBuildable* Buildable) override
	{
		if (AFGBuildableLightsControlPanel* Panel = FStructuralPowerBuildableCasts::AsPanel(Buildable))
		{
			FStructuralPanelConnectionPoint(Ctx.Graph(), Panel).OnWireOrGateChanged(
				EAttachContext::WireDelta);
		}
	}
};

class FStructuralPowerSwitchProcessorAdapter final : public IStructuralPowerProcessor
{
public:
	EStructuralEndpointKind GetBuildableKind() const override
	{
		return EStructuralEndpointKind::Switch;
	}

	EStructuralPowerRole GetPowerRole() const override
	{
		return EStructuralPowerRole::Router;
	}

	void Process(
		FStructuralPowerContext& Ctx,
		AFGBuildable* Buildable,
		bool bLocalPromoteOnly) override
	{
		if (AFGBuildableCircuitSwitch* Switch = FStructuralPowerBuildableCasts::AsSwitch(Buildable))
		{
			FStructuralPowerSwitchProcessor::Process(Ctx, Switch);
		}
	}

	void TearDown(FStructuralPowerContext& Ctx, AFGBuildable* Buildable) override
	{
		if (AFGBuildableCircuitSwitch* Switch = FStructuralPowerBuildableCasts::AsSwitch(Buildable))
		{
			FStructuralPowerSwitchProcessor::TearDown(Ctx, Switch);
		}
	}

	void OnWireDelta(FStructuralPowerContext& Ctx, AFGBuildable* Buildable) override
	{
		if (AFGBuildableCircuitSwitch* Switch = FStructuralPowerBuildableCasts::AsSwitch(Buildable))
		{
			FStructuralSwitchConnectionPoint(Ctx.Graph(), Switch).OnWireOrGateChanged(
				EAttachContext::WireDelta);
		}
	}
};

class FStructuralPowerPoleProcessorAdapter final : public IStructuralPowerProcessor
{
public:
	EStructuralEndpointKind GetBuildableKind() const override
	{
		return EStructuralEndpointKind::Pole;
	}

	EStructuralPowerRole GetPowerRole() const override
	{
		return EStructuralPowerRole::Router;
	}

	void Process(
		FStructuralPowerContext& Ctx,
		AFGBuildable* Buildable,
		bool bLocalPromoteOnly) override
	{
		if (AFGBuildablePowerPole* Pole = FStructuralPowerBuildableCasts::AsPole(Buildable))
		{
			FStructuralPowerPoleProcessor::Process(Ctx, Pole);
		}
	}

	void OnWireDelta(FStructuralPowerContext& Ctx, AFGBuildable* Buildable) override
	{
		if (AFGBuildablePowerPole* Pole = FStructuralPowerBuildableCasts::AsPole(Buildable))
		{
			FStructuralPoleConnectionPoint(Ctx.Graph(), Pole).OnWireOrGateChanged();
		}
	}
};

class FStructuralPowerStorageProcessorAdapter final : public IStructuralPowerProcessor
{
public:
	EStructuralEndpointKind GetBuildableKind() const override
	{
		return EStructuralEndpointKind::Storage;
	}

	EStructuralPowerRole GetPowerRole() const override
	{
		return EStructuralPowerRole::Host;
	}

	void Process(
		FStructuralPowerContext& Ctx,
		AFGBuildable* Buildable,
		bool bLocalPromoteOnly) override
	{
		if (AFGBuildablePowerStorage* Storage = FStructuralPowerBuildableCasts::AsStorage(Buildable))
		{
			FStructuralPowerStorageProcessor::Process(Ctx, Storage);
		}
	}

	void TearDown(FStructuralPowerContext& Ctx, AFGBuildable* Buildable) override
	{
		if (AFGBuildablePowerStorage* Storage = FStructuralPowerBuildableCasts::AsStorage(Buildable))
		{
			FStructuralPowerStorageProcessor::TearDown(Ctx, Storage);
		}
	}

	void OnWireDelta(FStructuralPowerContext& Ctx, AFGBuildable* Buildable) override
	{
		if (AFGBuildablePowerStorage* Storage = FStructuralPowerBuildableCasts::AsStorage(Buildable))
		{
			FStructuralStorageConnectionPoint(Ctx.Graph(), Storage).OnWireOrGateChanged(
				EAttachContext::WireDelta);
		}
	}
};

} // namespace

void FStructuralPowerProcessorRegistry::Register(
	TUniquePtr<IStructuralPowerProcessor> Processor)
{
	if (!Processor)
	{
		return;
	}

	ByKind.Add(Processor->GetBuildableKind(), MoveTemp(Processor));
}

void FStructuralPowerProcessorRegistry::Initialize()
{
	if (ByKind.Num() > 0)
	{
		return;
	}

	Register(MakeUnique<FStructuralPowerLightProcessorAdapter>());
	Register(MakeUnique<FStructuralPowerPanelProcessorAdapter>());
	Register(MakeUnique<FStructuralPowerSwitchProcessorAdapter>());
	Register(MakeUnique<FStructuralPowerPoleProcessorAdapter>());
	Register(MakeUnique<FStructuralPowerStorageProcessorAdapter>());
}
