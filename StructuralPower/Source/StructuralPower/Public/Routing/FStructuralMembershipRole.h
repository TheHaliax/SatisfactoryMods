// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Routing/EStructuralChannel.h"

class AFGBuildable;

enum class EStructuralMembershipRole : uint8 { None, Source, Control, Bridge };

class STRUCTURALPOWER_API FStructuralMembershipRole {
 public:
  static EStructuralMembershipRole Resolve(const AFGBuildable* Buildable,
                                           EStructuralChannel Tag = EStructuralChannel::Structure);

  static EStructuralMembershipRole ResolveFromEndpointKind(EStructuralEndpointKind Kind);

  static bool UsesKeyedMembership(EStructuralMembershipRole Role);

  static bool UsesSourceField(EStructuralMembershipRole Role);

  static bool UsesControlField(EStructuralMembershipRole Role);

  static bool PublishesControl(EStructuralMembershipRole Role);

  static bool JoinsViaSource(EStructuralMembershipRole Role);
};
