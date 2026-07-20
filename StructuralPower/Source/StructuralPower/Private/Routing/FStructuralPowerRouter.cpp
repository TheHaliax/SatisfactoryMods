// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Routing/FStructuralPowerRouter.h"

#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Config/FStructuralPowerModConfig.h"
#include "StructuralPowerConstants.h"

bool FStructuralPowerRouter::UsesSourceControlModel(EStructuralChannel Tag) {
  return Tag == EStructuralChannel::Light || Tag == EStructuralChannel::Switch ||
         Tag == EStructuralChannel::Generator;
}

bool FStructuralPowerRouter::IsStructuralGeneratorRoutingEnabled() {
  return FStructuralPowerModConfig::IsGroupGenerationEnabled();
}

bool FStructuralPowerRouter::IsGeneratorChannel(const EStructuralChannel Tag) {
  return Tag == EStructuralChannel::Generator;
}

bool FStructuralPowerRouter::UsesLegacyEffectiveIdModel(const EStructuralChannel Tag) {
  if (Tag == EStructuralChannel::Structure || UsesSourceControlModel(Tag)) {
    return false;
  }

  if (Tag == EStructuralChannel::Extractor) {
    return FStructuralPowerModConfig::IsGroupResourcesEnabled();
  }

  if (Tag == EStructuralChannel::Manufacturer) {
    return FStructuralPowerModConfig::IsGroupProductionEnabled();
  }

  if (Tag == EStructuralChannel::Transport) {
    return FStructuralPowerModConfig::IsGroupTransportEnabled();
  }

  if (Tag == EStructuralChannel::Misc) {
    return FStructuralPowerModConfig::IsGroupPipesEnabled() ||
           FStructuralPowerModConfig::IsGroupBeltsEnabled();
  }

  return false;
}

bool FStructuralPowerRouter::IsReservedSentinel(FName Id) {
  return Id == StructuralPowerConstants::ControlUnconfigured;
}

bool FStructuralPowerRouter::IsPlayerChosenIdValid(FName Id) {
  return !Id.IsNone() && !IsReservedSentinel(Id);
}

bool FStructuralPowerRouter::IsAssignedControl(FName Control) {
  return IsPlayerChosenIdValid(Control);
}

bool FStructuralPowerRouter::ShouldRouteChannelLink(const FStructuralChannelKey& A,
                                                    const FStructuralChannelKey& B,
                                                    const FStructuralComponentKey& ComponentA,
                                                    const FStructuralComponentKey& ComponentB) {
  if (!ComponentA.IsValid() || !ComponentB.IsValid() || ComponentA != ComponentB) {
    return false;
  }

  if (A.Tag != B.Tag || A.Tag == EStructuralChannel::Structure) {
    return false;
  }

  if (UsesSourceControlModel(A.Tag)) {
    return !A.Source.IsNone() && A.Source == B.Source;
  }

  if (UsesLegacyEffectiveIdModel(A.Tag)) {
    return !A.EffectiveId.IsNone() && A.EffectiveId == B.EffectiveId;
  }

  return !A.EffectiveId.IsNone() && A.EffectiveId == B.EffectiveId;
}

bool FStructuralPowerRouter::ShouldRouteKeyedJoin(FName PublisherControl, FName JoinerSource,
                                                  const FStructuralComponentKey& ComponentA,
                                                  const FStructuralComponentKey& ComponentB) {
  if (!ComponentA.IsValid() || ComponentA != ComponentB || !IsAssignedControl(PublisherControl) ||
      JoinerSource.IsNone()) {
    return false;
  }

  return PublisherControl == JoinerSource;
}

bool FStructuralPowerRouter::ShouldRouteSwitchGate(FName SwitchControl, FName DeviceSource,
                                                   const FStructuralComponentKey& ComponentA,
                                                   const FStructuralComponentKey& ComponentB) {
  return ShouldRouteKeyedJoin(SwitchControl, DeviceSource, ComponentA, ComponentB);
}

FName FStructuralPowerRouter::ResolveSwitchControlFromTag(const AFGBuildableCircuitSwitch* Switch) {
  if (!IsValid(Switch) || !Switch->HasBuildingTag_Implementation()) {
    return NAME_None;
  }

  const FString Tag = Switch->GetBuildingTag_Implementation();
  if (Tag.IsEmpty()) {
    return NAME_None;
  }

  return FName(*Tag);
}

FName FStructuralPowerRouter::MakeStructureDefaultIdName(const int32 StructureIndex) {
  if (StructureIndex < 1) {
    return NAME_None;
  }

  return FName(*FString::Printf(TEXT("Structure %d"), StructureIndex));
}

bool FStructuralPowerRouter::IsLegacyStructureDefaultId(const FName Id) {
  if (Id.IsNone()) {
    return false;
  }

  const FString AsString = Id.ToString();
  return AsString.StartsWith(TEXT("SP_"), ESearchCase::CaseSensitive);
}
