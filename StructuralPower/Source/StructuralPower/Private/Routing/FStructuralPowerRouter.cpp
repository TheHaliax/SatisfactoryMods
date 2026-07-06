// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Routing/FStructuralPowerRouter.h"

#include "Buildables/FGBuildableCircuitSwitch.h"
#include "StructuralPowerConstants.h"

bool FStructuralPowerRouter::UsesSourceControlModel(EStructuralChannel Tag)
{
	return Tag == EStructuralChannel::Light || Tag == EStructuralChannel::Switch;
}

bool FStructuralPowerRouter::IsReservedSentinel(FName Id)
{
	return Id == StructuralPowerConstants::ControlBypass
		|| Id == StructuralPowerConstants::ControlUnconfigured;
}

bool FStructuralPowerRouter::IsPlayerChosenIdValid(FName Id)
{
	return !Id.IsNone() && !IsReservedSentinel(Id);
}

bool FStructuralPowerRouter::ShouldRouteChannelLink(
	const FStructuralChannelKey& A,
	const FStructuralChannelKey& B,
	const FStructuralComponentKey& ComponentA,
	const FStructuralComponentKey& ComponentB)
{
	if (!ComponentA.IsValid() || !ComponentB.IsValid() || ComponentA != ComponentB)
	{
		return false;
	}

	if (A.Tag != B.Tag || A.Tag == EStructuralChannel::Structure)
	{
		return false;
	}

	if (UsesSourceControlModel(A.Tag))
	{
		return !A.Source.IsNone() && A.Source == B.Source;
	}

	return !A.EffectiveId.IsNone() && A.EffectiveId == B.EffectiveId;
}

bool FStructuralPowerRouter::ShouldRouteSwitchGate(
	FName SwitchControl,
	FName DeviceSource,
	const FStructuralComponentKey& ComponentA,
	const FStructuralComponentKey& ComponentB)
{
	if (!ComponentA.IsValid()
		|| ComponentA != ComponentB
		|| SwitchControl.IsNone()
		|| SwitchControl == StructuralPowerConstants::ControlBypass
		|| DeviceSource.IsNone())
	{
		return false;
	}

	return SwitchControl == DeviceSource;
}

FName FStructuralPowerRouter::ResolveSwitchControlFromTag(const AFGBuildableCircuitSwitch* Switch)
{
	if (!IsValid(Switch))
	{
		return StructuralPowerConstants::ControlBypass;
	}

	if (Switch->HasBuildingTag_Implementation())
	{
		const FString Tag = Switch->GetBuildingTag_Implementation();
		if (!Tag.IsEmpty())
		{
			return FName(*Tag);
		}
	}

	return StructuralPowerConstants::ControlBypass;
}

FName FStructuralPowerRouter::MakeDefaultIdName(const FStructuralNodeId& CanonicalNodeId)
{
	if (!CanonicalNodeId.IsValid())
	{
		return NAME_None;
	}

	if (CanonicalNodeId.IsLightweight())
	{
		return FName(*FString::Printf(
			TEXT("SP_%s_LW%d"),
			*CanonicalNodeId.BuildableClass.GetAssetName(),
			CanonicalNodeId.LightweightIndex));
	}

	return FName(*FString::Printf(
		TEXT("SP_%s_%s"),
		*CanonicalNodeId.BuildableClass.GetAssetName(),
		*CanonicalNodeId.ActorName.ToString()));
}
