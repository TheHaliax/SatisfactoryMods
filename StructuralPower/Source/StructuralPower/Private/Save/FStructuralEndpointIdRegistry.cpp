// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Save/FStructuralEndpointIdRegistry.h"

#include "Routing/FStructuralPowerRouter.h"

void FStructuralEndpointIdRegistry::Bind(
	TMap<FStructuralComponentKey, FName>& InComponentDefaultIds,
	TMap<FStructuralNodeId, FStructuralEndpointOverrides>& InPlayerEndpointOverrides)
{
	ComponentDefaultIds = &InComponentDefaultIds;
	PlayerEndpointOverrides = &InPlayerEndpointOverrides;
}

FName FStructuralEndpointIdRegistry::GetOrCreateComponentDefaultId(
	const FStructuralComponentKey& ComponentKey)
{
	if (!ComponentDefaultIds || !ComponentKey.IsValid())
	{
		return NAME_None;
	}

	if (const FName* Existing = ComponentDefaultIds->Find(ComponentKey))
	{
		return *Existing;
	}

	const FName Created = FStructuralPowerRouter::MakeDefaultIdName(ComponentKey.CanonicalNodeId);
	ComponentDefaultIds->Add(ComponentKey, Created);
	return Created;
}

bool FStructuralEndpointIdRegistry::TryGetPlayerOverride(
	const FStructuralNodeId& NodeId,
	FStructuralEndpointOverrides& Out) const
{
	Out = {};
	if (!PlayerEndpointOverrides)
	{
		return false;
	}

	if (const FStructuralEndpointOverrides* Found = PlayerEndpointOverrides->Find(NodeId))
	{
		Out = *Found;
		return Out.HasAnyOverride();
	}

	return false;
}

const FStructuralEndpointOverrides* FStructuralEndpointIdRegistry::FindPlayerOverride(
	const FStructuralNodeId& NodeId) const
{
	return PlayerEndpointOverrides ? PlayerEndpointOverrides->Find(NodeId) : nullptr;
}

void FStructuralEndpointIdRegistry::ApplyPlayerOverrideEdits(
	const FStructuralNodeId& NodeId,
	FName Source,
	FName Control,
	bool bClearSource,
	bool bClearControl)
{
	if (!PlayerEndpointOverrides)
	{
		return;
	}

	FStructuralEndpointOverrides& Entry = PlayerEndpointOverrides->FindOrAdd(NodeId);

	if (bClearSource)
	{
		Entry.SourceOverride = NAME_None;
	}
	else if (FStructuralPowerRouter::IsPlayerChosenIdValid(Source))
	{
		Entry.SourceOverride = Source;
	}

	if (bClearControl)
	{
		Entry.ControlOverride = NAME_None;
	}
	else if (FStructuralPowerRouter::IsPlayerChosenIdValid(Control))
	{
		Entry.ControlOverride = Control;
	}

	if (!Entry.HasAnyOverride())
	{
		PlayerEndpointOverrides->Remove(NodeId);
	}
}

void FStructuralEndpointIdRegistry::ForEachPlayerOverride(
	TFunctionRef<void(const FStructuralNodeId&, const FStructuralEndpointOverrides&)> Visitor) const
{
	if (!PlayerEndpointOverrides)
	{
		return;
	}

	for (const TPair<FStructuralNodeId, FStructuralEndpointOverrides>& Pair : *PlayerEndpointOverrides)
	{
		Visitor(Pair.Key, Pair.Value);
	}
}
