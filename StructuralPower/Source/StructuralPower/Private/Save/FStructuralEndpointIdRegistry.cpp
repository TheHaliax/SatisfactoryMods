// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Save/FStructuralEndpointIdRegistry.h"

#include "Routing/FStructuralPowerRouter.h"

void FStructuralEndpointIdRegistry::Bind(
    TMap<FStructuralComponentKey, FName>& InComponentDefaultIds,
    TMap<FStructuralNodeId, FStructuralEndpointOverrides>& InPlayerEndpointOverrides,
    int32& InNextStructureDefaultIdIndex) {
  ComponentDefaultIds = &InComponentDefaultIds;
  PlayerEndpointOverrides = &InPlayerEndpointOverrides;
  NextStructureDefaultIdIndex = &InNextStructureDefaultIdIndex;
  if (*NextStructureDefaultIdIndex < 1) {
    *NextStructureDefaultIdIndex = 1;
  }
}

FName FStructuralEndpointIdRegistry::AllocNextStructureDefaultId() {
  if (!NextStructureDefaultIdIndex) {
    return NAME_None;
  }

  const FName Created =
      FStructuralPowerRouter::MakeStructureDefaultIdName(*NextStructureDefaultIdIndex);
  ++(*NextStructureDefaultIdIndex);
  return Created;
}

FName FStructuralEndpointIdRegistry::GetOrCreateComponentDefaultId(
    const FStructuralComponentKey& ComponentKey) {
  if (!ComponentDefaultIds || !ComponentKey.IsValid()) {
    return NAME_None;
  }

  if (FName* Existing = ComponentDefaultIds->Find(ComponentKey)) {
    if (FStructuralPowerRouter::IsLegacyStructureDefaultId(*Existing)) {
      *Existing = AllocNextStructureDefaultId();
    }
    return *Existing;
  }

  const FName Created = AllocNextStructureDefaultId();
  ComponentDefaultIds->Add(ComponentKey, Created);
  return Created;
}

bool FStructuralEndpointIdRegistry::TryGetComponentDefaultId(
    const FStructuralComponentKey& ComponentKey, FName& OutId) const {
  OutId = NAME_None;
  if (!ComponentDefaultIds || !ComponentKey.IsValid()) {
    return false;
  }

  if (const FName* Existing = ComponentDefaultIds->Find(ComponentKey)) {
    OutId = *Existing;
    return !OutId.IsNone();
  }

  return false;
}

void FStructuralEndpointIdRegistry::MigrateLegacyStructureDefaultIds() {
  if (!ComponentDefaultIds) {
    return;
  }

  for (TPair<FStructuralComponentKey, FName>& Pair : *ComponentDefaultIds) {
    if (FStructuralPowerRouter::IsLegacyStructureDefaultId(Pair.Value)) {
      Pair.Value = AllocNextStructureDefaultId();
    }
  }
}

bool FStructuralEndpointIdRegistry::TryGetPlayerOverride(const FStructuralNodeId& NodeId,
                                                         FStructuralEndpointOverrides& Out) const {
  Out = {};
  if (!PlayerEndpointOverrides) {
    return false;
  }

  if (const FStructuralEndpointOverrides* Found = PlayerEndpointOverrides->Find(NodeId)) {
    Out = *Found;
    return Out.HasAnyOverride();
  }

  return false;
}

const FStructuralEndpointOverrides*
FStructuralEndpointIdRegistry::FindPlayerOverride(const FStructuralNodeId& NodeId) const {
  return PlayerEndpointOverrides ? PlayerEndpointOverrides->Find(NodeId) : nullptr;
}

void FStructuralEndpointIdRegistry::ApplyPlayerOverrideEdits(const FStructuralNodeId& NodeId,
                                                             FName Source, FName Control,
                                                             bool bClearSource, bool bClearControl,
                                                             const bool bGlobalControl,
                                                             const bool bTouchGlobalControl) {
  if (!PlayerEndpointOverrides) {
    return;
  }

  FStructuralEndpointOverrides& Entry = PlayerEndpointOverrides->FindOrAdd(NodeId);

  if (bClearSource) {
    Entry.SourceOverride = NAME_None;
  } else if (FStructuralPowerRouter::IsPlayerChosenIdValid(Source)) {
    Entry.SourceOverride = Source;
  }

  if (bClearControl) {
    Entry.ControlOverride = NAME_None;
    Entry.bGlobalControl = false;
  } else if (FStructuralPowerRouter::IsPlayerChosenIdValid(Control)) {
    Entry.ControlOverride = Control;
  }

  if (bTouchGlobalControl) {
    Entry.bGlobalControl = bGlobalControl && !Entry.ControlOverride.IsNone();
  }

  if (!Entry.HasAnyOverride()) {
    PlayerEndpointOverrides->Remove(NodeId);
  }
}

void FStructuralEndpointIdRegistry::ForEachPlayerOverride(
    TFunctionRef<void(const FStructuralNodeId&, const FStructuralEndpointOverrides&)> Visitor)
    const {
  if (!PlayerEndpointOverrides) {
    return;
  }

  for (const TPair<FStructuralNodeId, FStructuralEndpointOverrides>& Pair :
       *PlayerEndpointOverrides) {
    Visitor(Pair.Key, Pair.Value);
  }
}
