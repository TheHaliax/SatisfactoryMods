// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "Core/FStructuralNodeId.h"
#include "CoreMinimal.h"
#include "Routing/EStructuralChannel.h"

class AFGBuildable;

enum class EStructuralEndpointKind : uint8 {
  Pole,
  Switch,
  Light,
  Panel,
  Storage,
  Generator,
  Extractor,
  Manufacturer,
  Transport,
  PipePump
};

STRUCTURALPOWER_API const TCHAR* StructuralEndpointKindToString(EStructuralEndpointKind Kind);

struct FTrackedEndpoint {
  TWeakObjectPtr<AFGBuildable> Actor;
  FStructuralNodeId MountParentId;
  EStructuralEndpointKind Kind = EStructuralEndpointKind::Pole;

  FStructuralChannelKey CachedPanelKey;
  int32 CachedPanelRoot = INDEX_NONE;
  bool bPanelLinksReady = false;

  FName CachedDownstreamControl = NAME_None;
  bool bDownstreamLinksReady = false;

  uint8 CachedSwitchWireSignature = 0xFF;

  bool bStructuralPowerTransferActive = false;
  bool bAwaitingStructuralSite = false;
  /** PipePump only — bounded deferred inject resolve after place race. */
  uint8 PipeInjectResolveAttempts = 0;

  class AFGBuildableCircuitSwitch* GetSwitch() const;
  class AFGBuildablePowerPole* GetPole() const;
  class AFGBuildableLightSource* GetLight() const;
  class AFGBuildableLightsControlPanel* GetPanel() const;
  class AFGBuildablePowerStorage* GetStorage() const;
  class AFGBuildableGenerator* GetGenerator() const;
};
