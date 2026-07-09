// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Save/FStructuralGraphIdOps.h"

#include "Save/AStructuralPowerGraphSubsystem.h"

#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Buildables/FGBuildableLightSource.h"
#include "Buildables/FGBuildableGenerator.h"
#include "Buildables/FGBuildableLightsControlPanel.h"
#include "Core/FStructuralPowerContext.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Graph/FStructuralPowerBuildableCasts.h"
#include "Save/FStructuralPlacementQueue.h"
#include "Graph/FStructuralAttachmentResolver.h"
#include "Processors/FStructuralPowerProcessorRegistry.h"
#include "Processors/FStructuralPowerSwitchProcessor.h"
#include "Processors/IStructuralPowerProcessor.h"
#include "Routing/FStructuralMembershipRole.h"
#include "Routing/FStructuralPowerRouter.h"
#include "Rules/FStructuralEligibilityRules.h"
#include "StructuralPowerConstants.h"

void FStructuralGraphIdOps::Bind(AStructuralPowerGraphSubsystem* InSubsystem)
{
	Subsystem = InSubsystem;
}

FStructuralComponentKey FStructuralGraphIdOps::MakeComponentKeyForRoot(int32 ComponentRoot) const
{
	FStructuralComponentKey Key;
	Key.CanonicalNodeId = Subsystem->StructureGraph.MakeCanonicalNodeIdForComponent(ComponentRoot);
	return Key;
}

FStructuralComponentKey FStructuralGraphIdOps::MakeComponentKeyForParent(
	const FStructuralNodeId& ParentId) const
{
	return MakeComponentKeyForRoot(Subsystem->StructureGraph.FindRoot(ParentId));
}

FStructuralComponentKey FStructuralGraphIdOps::MakeComponentKeyForBuildable(
	const AFGBuildable* Buildable) const
{
	if (!IsValid(Buildable))
	{
		return {};
	}

	const FStructuralNodeId BuildableId = AStructuralPowerGraphSubsystem::MakeNodeId(Buildable);
	if (const FTrackedEndpoint* Endpoint = Subsystem->TrackedEndpoints.Find(BuildableId))
	{
		return MakeComponentKeyForParent(Endpoint->ParentId);
	}

	const FStructuralWallAnchor Anchor = FStructuralAttachmentResolver::ResolveStructuralParent(
		const_cast<AFGBuildable*>(Buildable),
		Subsystem->GetWorld(),
		Subsystem->LightweightIndex);
	if (Anchor.IsValid())
	{
		return MakeComponentKeyForParent(AStructuralPowerGraphSubsystem::MakeParentNodeId(Anchor));
	}

	return {};
}

bool FStructuralGraphIdOps::GetEndpointOverrides(
	const AFGBuildable* Buildable,
	FStructuralEndpointOverrides& Out) const
{
	Out = {};
	if (!IsValid(Buildable))
	{
		return false;
	}

	const FStructuralNodeId BuildableId = AStructuralPowerGraphSubsystem::MakeNodeId(Buildable);
	if (const FStructuralEndpointOverrides* Found = Subsystem->IdRegistry.FindPlayerOverride(BuildableId))
	{
		Out = *Found;
		return Out.HasAnyOverride();
	}

	return false;
}

FName FStructuralGraphIdOps::GetOrCreateComponentDefaultId(
	const FStructuralComponentKey& ComponentKey)
{
	return Subsystem->IdRegistry.GetOrCreateComponentDefaultId(ComponentKey);
}

FName FStructuralGraphIdOps::ResolveSource(
	AFGBuildable* Buildable,
	EStructuralChannel Tag)
{
	if (!IsValid(Buildable) || Tag == EStructuralChannel::Structure)
	{
		return NAME_None;
	}

	const EStructuralMembershipRole Role = FStructuralMembershipRole::Resolve(Buildable, Tag);
	if (Role == EStructuralMembershipRole::Source)
	{
		return NAME_None;
	}

	const FStructuralNodeId BuildableId = AStructuralPowerGraphSubsystem::MakeNodeId(Buildable);
	if (FStructuralPowerRouter::UsesSourceControlModel(Tag)
		|| FStructuralMembershipRole::UsesKeyedMembership(Role))
	{
		if (const FStructuralEndpointOverrides* Overrides =
				Subsystem->IdRegistry.FindPlayerOverride(BuildableId);
			Overrides && !Overrides->SourceOverride.IsNone())
		{
			return Overrides->SourceOverride;
		}

		if (const FStructuralComponentKey ComponentKey = MakeComponentKeyForBuildable(Buildable);
			ComponentKey.IsValid())
		{
			return GetOrCreateComponentDefaultId(ComponentKey);
		}

		return NAME_None;
	}

	if (const FStructuralEndpointOverrides* Overrides =
			Subsystem->IdRegistry.FindPlayerOverride(BuildableId);
		Overrides && !Overrides->SourceOverride.IsNone())
	{
		return Overrides->SourceOverride;
	}

	if (Tag == EStructuralChannel::Switch)
	{
		if (const AFGBuildableCircuitSwitch* Switch = Cast<AFGBuildableCircuitSwitch>(Buildable))
		{
			const FName Control = FStructuralPowerRouter::ResolveSwitchControlFromTag(Switch);
			if (FStructuralPowerRouter::IsAssignedControl(Control))
			{
				return Control;
			}
		}
	}

	if (const FStructuralComponentKey ComponentKey = MakeComponentKeyForBuildable(Buildable);
		ComponentKey.IsValid())
	{
		return GetOrCreateComponentDefaultId(ComponentKey);
	}

	return NAME_None;
}

FName FStructuralGraphIdOps::ResolveControl(
	AFGBuildable* Buildable,
	EStructuralChannel Tag)
{
	if (!IsValid(Buildable))
	{
		return NAME_None;
	}

	const EStructuralMembershipRole Role = FStructuralMembershipRole::Resolve(Buildable, Tag);
	if (Role == EStructuralMembershipRole::Control)
	{
		return NAME_None;
	}

	if (!FStructuralPowerRouter::UsesSourceControlModel(Tag)
		&& !FStructuralMembershipRole::PublishesControl(Role))
	{
		return NAME_None;
	}

	const FStructuralNodeId BuildableId = AStructuralPowerGraphSubsystem::MakeNodeId(Buildable);
	if (const FStructuralEndpointOverrides* Overrides =
			Subsystem->IdRegistry.FindPlayerOverride(BuildableId);
		Overrides && !Overrides->ControlOverride.IsNone())
	{
		if (Overrides->ControlOverride == FName(TEXT("BYPASS")))
		{
			return NAME_None;
		}
		return Overrides->ControlOverride;
	}

	if (Tag == EStructuralChannel::Switch)
	{
		if (const AFGBuildableCircuitSwitch* Switch = Cast<AFGBuildableCircuitSwitch>(Buildable))
		{
			return FStructuralPowerRouter::ResolveSwitchControlFromTag(Switch);
		}

		return NAME_None;
	}

	if (Buildable->IsA<AFGBuildableLightsControlPanel>())
	{
		return StructuralPowerConstants::ControlUnconfigured;
	}

	if (Role == EStructuralMembershipRole::Source)
	{
		if (const FStructuralComponentKey ComponentKey = MakeComponentKeyForBuildable(Buildable);
			ComponentKey.IsValid())
		{
			return GetOrCreateComponentDefaultId(ComponentKey);
		}
	}

	return NAME_None;
}

FStructuralChannelKey FStructuralGraphIdOps::ResolveChannelKeyForBuildable(
	AFGBuildable* Buildable)
{
	FStructuralChannelKey Key;
	if (!IsValid(Buildable))
	{
		return Key;
	}

	Key.Tag = FStructuralEligibilityRules::ClassifyBuildable(Buildable);
	const EStructuralMembershipRole Role = FStructuralMembershipRole::Resolve(Buildable, Key.Tag);
	if (FStructuralPowerRouter::UsesSourceControlModel(Key.Tag)
		|| FStructuralMembershipRole::UsesKeyedMembership(Role))
	{
		Key.Source = ResolveSource(Buildable, Key.Tag);
		Key.Control = ResolveControl(Buildable, Key.Tag);
		Key.EffectiveId = Role == EStructuralMembershipRole::Source ? Key.Control : Key.Source;
	}
	else
	{
		Key.EffectiveId = ResolveSource(Buildable, Key.Tag);
	}

	return Key;
}

void FStructuralGraphIdOps::SetEndpointIds(
	AFGBuildable* Buildable,
	FName Source,
	FName Control,
	bool bClearSource,
	bool bClearControl)
{
	if (!IsValid(Buildable) || !Buildable->HasAuthority())
	{
		return;
	}

	const FStructuralNodeId NodeId = AStructuralPowerGraphSubsystem::MakeNodeId(Buildable);
	Subsystem->IdRegistry.ApplyPlayerOverrideEdits(
		NodeId,
		Source,
		Control,
		bClearSource,
		bClearControl);

	const bool bIsLight = Buildable->IsA<AFGBuildableLightSource>();
	const bool bIsPanel = Buildable->IsA<AFGBuildableLightsControlPanel>();
	const bool bIsSwitch = Buildable->IsA<AFGBuildableCircuitSwitch>();
	const bool bGroupLighting = FStructuralPowerModConfig::IsGroupLightingEnabled();

	const bool bSkipDeferredOutlet = (bGroupLighting && (bIsLight || bIsPanel)) || bIsSwitch;
	if (!bSkipDeferredOutlet)
	{
		Subsystem->EnqueuePlacement(
			Buildable,
			EStructuralPlacementJobType::Outlet,
			/*bDefer=*/true);
	}

	const FStructuralWallAnchor Anchor = Subsystem->ResolveOutletAnchor(Buildable);
	FStructuralNodeId ParentId;
	const int32 Root = Subsystem->BridgeRootIndex.ResolveEndpointComponentRoot(
		Buildable,
		Anchor,
		ParentId);
	if (Root == INDEX_NONE)
	{
		return;
	}

	if (bIsLight && bGroupLighting)
	{
		if (AFGBuildableLightSource* Light = FStructuralPowerBuildableCasts::AsLight(Buildable))
		{
			const FStructuralChannelKey LightKey =
				Subsystem->IdOps.ResolveChannelKeyForBuildable(Light);
			Subsystem->ProcessLightEndpoint(Light);
			if (Subsystem->IsPanelDownstreamLight(Root, LightKey))
			{
				Subsystem->RefreshPanelsForLightSourceOnRoot(Root, LightKey.Source);
			}
		}
	}
	else if (bIsPanel && bGroupLighting)
	{
		if (AFGBuildableLightsControlPanel* Panel = FStructuralPowerBuildableCasts::AsPanel(Buildable))
		{
			FTrackedEndpoint& Tracked = Subsystem->TrackedEndpoints.FindOrAdd(NodeId);
			Tracked.bPanelLinksReady = false;
			Tracked.bDownstreamLinksReady = false;
			Subsystem->ProcessPanelEndpoint(Panel);
		}
	}
	else if (bIsSwitch)
	{
		if (AFGBuildableCircuitSwitch* Switch = FStructuralPowerBuildableCasts::AsSwitch(Buildable))
		{
			FStructuralPowerContext Ctx =
				Subsystem->MakeProcessorContext(EAttachContext::RuntimePlace, Root);
			if (IStructuralPowerProcessor* Processor =
					FStructuralPowerProcessorRegistry::Get().FindMutable(EStructuralEndpointKind::Switch))
			{
				Processor->Process(Ctx, Switch);
			}
		}
	}

	Subsystem->RebuildControlIdGangsForRoot(Root);
}

bool FStructuralGraphIdOps::CollectIdsOnComponent(
	const FStructuralComponentKey& Key,
	FStructuralComponentIdList& Out) const
{
	if (!Key.IsValid())
	{
		return false;
	}

	const int32 TargetRoot = Subsystem->StructureGraph.FindRoot(Key.CanonicalNodeId);
	if (TargetRoot == INDEX_NONE)
	{
		return false;
	}

	Out = {};
	Out.DefaultSourceId = const_cast<FStructuralGraphIdOps*>(this)
		->GetOrCreateComponentDefaultId(Key);

	TSet<FName> NamedSources;
	TSet<FName> NamedControls;
	TSet<FName> NamedSwitchControls;
	TSet<FName> NamedLightGroups;

	auto ConsiderBuildable = [&](const FStructuralNodeId& NodeId, const AFGBuildable* Buildable)
	{
		if (!IsValid(Buildable))
		{
			return;
		}

		const FStructuralComponentKey BuildableKey = MakeComponentKeyForBuildable(Buildable);
		if (!BuildableKey.IsValid()
			|| Subsystem->StructureGraph.FindRoot(BuildableKey.CanonicalNodeId) != TargetRoot)
		{
			return;
		}

		const bool bIsLight = FStructuralEligibilityRules::IsStructuralLightConsumer(Buildable);
		const bool bIsPanel = Buildable->IsA<AFGBuildableLightsControlPanel>();
		const bool bIsSwitch = Buildable->IsA<AFGBuildableCircuitSwitch>();
		const bool bIsGenerator = Buildable->IsA<AFGBuildableGenerator>();

		if (const FStructuralEndpointOverrides* Overrides = Subsystem->IdRegistry.FindPlayerOverride(NodeId))
		{
			if (!Overrides->SourceOverride.IsNone()
				&& Overrides->SourceOverride != Out.DefaultSourceId)
			{
				NamedSources.Add(Overrides->SourceOverride);
			}

			if (!Overrides->ControlOverride.IsNone()
				&& !FStructuralPowerRouter::IsReservedSentinel(Overrides->ControlOverride))
			{
				NamedControls.Add(Overrides->ControlOverride);
				if (bIsPanel)
				{
					NamedLightGroups.Add(Overrides->ControlOverride);
				}
				else if (bIsSwitch)
				{
					NamedSwitchControls.Add(Overrides->ControlOverride);
				}
				else if (bIsGenerator)
				{
					NamedControls.Add(Overrides->ControlOverride);
				}
			}
		}

		if (bIsSwitch)
		{
			const FName TagControl =
				FStructuralPowerRouter::ResolveSwitchControlFromTag(
					Cast<AFGBuildableCircuitSwitch>(Buildable));
			if (!FStructuralPowerRouter::IsReservedSentinel(TagControl))
			{
				NamedControls.Add(TagControl);
				NamedSwitchControls.Add(TagControl);
			}
		}

		const EStructuralChannel Tag = FStructuralEligibilityRules::ClassifyBuildable(Buildable);
		FStructuralGraphIdOps* MutableThis =
			const_cast<FStructuralGraphIdOps*>(this);
		AFGBuildable* MutableBuildable = const_cast<AFGBuildable*>(Buildable);
		const FName ResolvedSource = MutableThis->ResolveSource(MutableBuildable, Tag);
		const FName ResolvedControl = MutableThis->ResolveControl(MutableBuildable, Tag);

		if (FStructuralPowerRouter::IsPlayerChosenIdValid(ResolvedSource)
			&& ResolvedSource != Out.DefaultSourceId
			&& !bIsLight)
		{
			NamedSources.Add(ResolvedSource);
		}

		if (FStructuralPowerRouter::IsPlayerChosenIdValid(ResolvedControl)
			&& !FStructuralPowerRouter::IsReservedSentinel(ResolvedControl))
		{
			NamedControls.Add(ResolvedControl);
			if (bIsPanel)
			{
				NamedLightGroups.Add(ResolvedControl);
			}
			else if (bIsSwitch)
			{
				NamedSwitchControls.Add(ResolvedControl);
			}
		}
	};

	for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : Subsystem->TrackedEndpoints)
	{
		ConsiderBuildable(Pair.Key, Pair.Value.Actor.Get());
	}

	for (const TPair<FStructuralNodeId, TWeakObjectPtr<AFGBuildable>>& Pair :
		Subsystem->RegisteredBuildables)
	{
		ConsiderBuildable(Pair.Key, Pair.Value.Get());
	}

	Subsystem->IdRegistry.ForEachPlayerOverride(
		[&](const FStructuralNodeId& NodeId, const FStructuralEndpointOverrides&)
	{
		if (const TWeakObjectPtr<AFGBuildable>* Buildable = Subsystem->RegisteredBuildables.Find(NodeId))
		{
			ConsiderBuildable(NodeId, Buildable->Get());
		}
	});

	Out.NamedControlIds = NamedControls.Array();
	Out.NamedSwitchControlIds = NamedSwitchControls.Array();
	Out.NamedLightGroupIds = NamedLightGroups.Array();

	for (const TPair<FStructuralNodeId, FTrackedEndpoint>& Pair : Subsystem->TrackedEndpoints)
	{
		if (Pair.Value.Kind != EStructuralEndpointKind::Light)
		{
			continue;
		}

		if (Subsystem->StructureGraph.FindRoot(Pair.Value.ParentId) != TargetRoot)
		{
			continue;
		}

		AFGBuildableLightSource* Light = Cast<AFGBuildableLightSource>(Pair.Value.Actor.Get());
		if (!IsValid(Light))
		{
			continue;
		}

		const FName LightSource = const_cast<FStructuralGraphIdOps*>(this)
			->ResolveSource(Light, EStructuralChannel::Light);
		if (!FStructuralPowerRouter::IsPlayerChosenIdValid(LightSource)
			|| LightSource == Out.DefaultSourceId)
		{
			continue;
		}

		if (NamedLightGroups.Contains(LightSource))
		{
			continue;
		}

		NamedSources.Add(LightSource);
	}

	for (const FName& LightGroupId : NamedLightGroups)
	{
		NamedSources.Remove(LightGroupId);
	}

	Out.NamedSourceIds = NamedSources.Array();
	Out.NamedSourceIds.Sort(FNameLexicalLess());
	Out.NamedControlIds.Sort(FNameLexicalLess());
	Out.NamedSwitchControlIds.Sort(FNameLexicalLess());
	Out.NamedLightGroupIds.Sort(FNameLexicalLess());
	return true;
}
