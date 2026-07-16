// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Routing/FStructuralMembershipRole.h"

#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Buildables/FGBuildableGenerator.h"
#include "Buildables/FGBuildableLightSource.h"
#include "Buildables/FGBuildableLightsControlPanel.h"
#include "Rules/FStructuralEligibilityRules.h"

EStructuralMembershipRole FStructuralMembershipRole::Resolve(const AFGBuildable* Buildable,
                                                             EStructuralChannel Tag) {
  if (!IsValid(Buildable)) {
    return EStructuralMembershipRole::None;
  }

  if (Tag == EStructuralChannel::Structure) {
    Tag = FStructuralEligibilityRules::ClassifyBuildable(Buildable);
  }

  if (Buildable->IsA<AFGBuildableCircuitSwitch>() ||
      Buildable->IsA<AFGBuildableLightsControlPanel>()) {
    return EStructuralMembershipRole::Bridge;
  }

  if (Buildable->IsA<AFGBuildableGenerator>() || Tag == EStructuralChannel::Generator) {
    return EStructuralMembershipRole::Source;
  }

  if (FStructuralEligibilityRules::IsStructuralLightConsumer(Buildable) ||
      Tag == EStructuralChannel::Light || Tag == EStructuralChannel::Extractor ||
      Tag == EStructuralChannel::Manufacturer || Tag == EStructuralChannel::Transport ||
      Tag == EStructuralChannel::Misc) {
    return EStructuralMembershipRole::Control;
  }

  return EStructuralMembershipRole::None;
}

EStructuralMembershipRole FStructuralMembershipRole::ResolveFromEndpointKind(
    const EStructuralEndpointKind Kind) {
  switch (Kind) {
    case EStructuralEndpointKind::Switch:
    case EStructuralEndpointKind::Panel:
      return EStructuralMembershipRole::Bridge;
    case EStructuralEndpointKind::Generator:
      return EStructuralMembershipRole::Source;
    case EStructuralEndpointKind::Light:
    case EStructuralEndpointKind::Pole:
    case EStructuralEndpointKind::Storage:
    case EStructuralEndpointKind::Extractor:
    case EStructuralEndpointKind::Manufacturer:
    case EStructuralEndpointKind::Transport:
    case EStructuralEndpointKind::PipePump:
      return EStructuralMembershipRole::Control;
    default:
      return EStructuralMembershipRole::None;
  }
}

bool FStructuralMembershipRole::UsesKeyedMembership(const EStructuralMembershipRole Role) {
  return Role != EStructuralMembershipRole::None;
}

bool FStructuralMembershipRole::UsesSourceField(const EStructuralMembershipRole Role) {
  return Role == EStructuralMembershipRole::Control || Role == EStructuralMembershipRole::Bridge;
}

bool FStructuralMembershipRole::UsesControlField(const EStructuralMembershipRole Role) {
  return Role == EStructuralMembershipRole::Source || Role == EStructuralMembershipRole::Bridge;
}

bool FStructuralMembershipRole::PublishesControl(const EStructuralMembershipRole Role) {
  return Role == EStructuralMembershipRole::Source || Role == EStructuralMembershipRole::Bridge;
}

bool FStructuralMembershipRole::JoinsViaSource(const EStructuralMembershipRole Role) {
  return Role == EStructuralMembershipRole::Control || Role == EStructuralMembershipRole::Bridge;
}
