// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "Core/EStructuralAttachStrategy.h"
#include "Core/EStructuralPowerRole.h"
#include "CoreMinimal.h"
#include "Graph/FStructuralEndpointTypes.h"
#include "Routing/EStructuralChannel.h"
#include "Save/FStructuralPlacementQueue.h"

class AFGBuildable;

struct STRUCTURALPOWER_API FStructuralEndpointDescriptor {
  EStructuralEndpointKind Kind = EStructuralEndpointKind::Pole;
  EStructuralChannel Channel = EStructuralChannel::Structure;
  EStructuralPowerRole Role = EStructuralPowerRole::Gateway;
  EStructuralAttachStrategy AttachStrategy = EStructuralAttachStrategy::Bridge;
  EStructuralPlacementJobType PlacementJob = EStructuralPlacementJobType::Outlet;
  bool bParticipatesInSiteMesh = true;
  bool (*Classifier)(const AFGBuildable*) = nullptr;
  bool (*GroupGate)() = nullptr;
};
