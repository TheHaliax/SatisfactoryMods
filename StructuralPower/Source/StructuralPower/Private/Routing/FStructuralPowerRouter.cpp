// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Routing/FStructuralPowerRouter.h"

#include "Buildables/FGBuildableCircuitSwitch.h"

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

	return !A.EffectiveId.IsNone() && A.EffectiveId == B.EffectiveId;
}

bool FStructuralPowerRouter::ShouldRouteSwitchGate(
	FName SwitchEffectiveId,
	FName DeviceEffectiveId,
	const FStructuralComponentKey& ComponentA,
	const FStructuralComponentKey& ComponentB)
{
	return ComponentA.IsValid()
		&& ComponentA == ComponentB
		&& !SwitchEffectiveId.IsNone()
		&& SwitchEffectiveId == DeviceEffectiveId;
}

FName FStructuralPowerRouter::ResolveSwitchEffectiveId(const AFGBuildableCircuitSwitch* Switch)
{
	if (!IsValid(Switch))
	{
		return NAME_None;
	}

	if (Switch->HasBuildingTag_Implementation())
	{
		const FString Tag = Switch->GetBuildingTag_Implementation();
		if (!Tag.IsEmpty())
		{
			return FName(*Tag);
		}
	}

	return NAME_None;
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
