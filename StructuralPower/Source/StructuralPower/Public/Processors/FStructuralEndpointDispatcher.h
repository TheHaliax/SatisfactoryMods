// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "Core/EAttachContext.h"
#include "CoreMinimal.h"

class AFGBuildable;
class AFGBuildableCircuitSwitch;
class AFGBuildableLightsControlPanel;
class AFGBuildablePowerPole;
class FStructuralGraphSession;
class IStructuralEndpointProcessor;
struct FStructuralPowerContext;

class STRUCTURALPOWER_API FStructuralEndpointDispatcher {
 public:
  static void RunPlacement(FStructuralGraphSession& Session,
                           IStructuralEndpointProcessor& Processor, AFGBuildable* Host,
                           bool bLocalPromoteOnly = false, bool bOverrideAttachContext = false,
                           EAttachContext AttachContextOverride = EAttachContext::RuntimePlace);

  static void RunWireDelta(FStructuralGraphSession& Session,
                           IStructuralEndpointProcessor& Processor, AFGBuildable* Host,
                           EAttachContext AttachContext = EAttachContext::WireDelta);

  static void RunCircuitsRebuilt(FStructuralGraphSession& Session,
                                 IStructuralEndpointProcessor& Processor, AFGBuildable* Host,
                                 bool bOverrideAttachContext = false,
                                 EAttachContext AttachContextOverride = EAttachContext::WireDelta,
                                 int32 SiteRoot = INDEX_NONE);

  static void RunToggleChanged(FStructuralGraphSession& Session,
                               IStructuralEndpointProcessor& Processor, AFGBuildable* Host);

  static void RunTeardown(FStructuralGraphSession& Session, IStructuralEndpointProcessor& Processor,
                          AFGBuildable* Host);

  static void
  DispatchPlacement(FStructuralGraphSession& Session, AFGBuildable* Buildable,
                    bool bLocalPromoteOnly = false, bool bOverrideAttachContext = false,
                    EAttachContext AttachContextOverride = EAttachContext::RuntimePlace);

  static void DispatchOutlet(FStructuralGraphSession& Session, AFGBuildable* Buildable);

  static void DispatchTeardown(FStructuralGraphSession& Session, AFGBuildable* Buildable);

  static void DispatchPoleWireDelta(FStructuralGraphSession& Session, AFGBuildablePowerPole* Pole);

  static void DispatchSwitchCircuitsRebuilt(FStructuralGraphSession& Session,
                                            AFGBuildableCircuitSwitch* Switch);

  static void DispatchPanelWireDelta(FStructuralGraphSession& Session,
                                     AFGBuildableLightsControlPanel* Panel);

  static void DispatchWallOutletAfterWire(FStructuralGraphSession& Session,
                                          AFGBuildablePowerPole* Pole);

  static bool ShouldSkipOutletDuringBulkLoad(const FStructuralGraphSession& Session,
                                             const AFGBuildable* Buildable);
};
