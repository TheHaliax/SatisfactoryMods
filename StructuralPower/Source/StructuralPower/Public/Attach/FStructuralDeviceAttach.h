// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Routing/EStructuralChannel.h"

class AFGBuildable;
class AFGBuildableLightSource;
class FStructuralGraphSession;
class UFGPowerConnectionComponent;

class STRUCTURALPOWER_API FStructuralDeviceAttach {
 public:
  /** DR-007: match GetPowerInfo() to first power info, visible-plug fallback. */
  static UFGPowerConnectionComponent* FindFactoryWireConnection(const AFGBuildable* Host);

  static UFGPowerConnectionComponent* FindLightWireConnection(const AFGBuildableLightSource* Light);

  static bool TryAttachConsumer(FStructuralGraphSession& Session, AFGBuildable* Device,
                                UFGPowerConnectionComponent* DevicePlug, int32 ComponentRoot,
                                const FStructuralChannelKey& DeviceKey);

  /** Source-role supply inject — default component bus feed (gen Control is gang key). */
  static bool TryAttachSourceSupply(FStructuralGraphSession& Session, AFGBuildable* Device,
                                    UFGPowerConnectionComponent* DevicePlug, int32 ComponentRoot);

  static void TearDownConsumerLinks(UFGPowerConnectionComponent* DevicePlug);
};
