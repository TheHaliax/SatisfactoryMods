// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Attach/FStructuralDeviceAttach.h"

#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableLightSource.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "Circuit/FStructuralCircuitPromotionUtil.h"
#include "Circuit/FStructuralGraphCircuitOps.h"
#include "Components/UFGStructuralPowerConnectionComponent.h"
#include "Config/FStructuralPowerModConfig.h"
#include "Core/FStructuralGraphSession.h"
#include "FGPowerConnectionComponent.h"
#include "FGPowerInfoComponent.h"
#include "Routing/FStructuralPowerRouter.h"
#include "Save/AStructuralPowerGraphSubsystem.h"

UFGPowerConnectionComponent* FStructuralDeviceAttach::FindFactoryWireConnection(
    const AFGBuildable* Host) {
  if (!IsValid(Host)) {
    return nullptr;
  }

  TInlineComponentArray<UFGPowerInfoComponent*> Infos(Host);
  UFGPowerInfoComponent* const PowerInfo = Infos.Num() > 0 ? Infos[0] : nullptr;

  TInlineComponentArray<UFGPowerConnectionComponent*> Conns(Host);
  for (UFGPowerConnectionComponent* Conn : Conns) {
    if (!IsValid(Conn) || Conn->IsHidden()) {
      continue;
    }

    if (PowerInfo && Conn->GetPowerInfo() == PowerInfo) {
      return Conn;
    }
  }

  for (UFGPowerConnectionComponent* Conn : Conns) {
    if (IsValid(Conn) && !Conn->IsHidden()) {
      return Conn;
    }
  }

  return nullptr;
}

UFGPowerConnectionComponent* FStructuralDeviceAttach::FindLightWireConnection(
    const AFGBuildableLightSource* Light) {
  return FindFactoryWireConnection(Light);
}

bool FStructuralDeviceAttach::TryAttachConsumer(FStructuralGraphSession& Session,
                                                AFGBuildable* Device,
                                                UFGPowerConnectionComponent* DevicePlug,
                                                int32 ComponentRoot,
                                                const FStructuralChannelKey& DeviceKey) {
  if (!IsValid(Device) || !IsValid(DevicePlug) || ComponentRoot == INDEX_NONE) {
    return false;
  }

  const FStructuralComponentKey CompKey = Session.Ids().MakeComponentKeyForRoot(ComponentRoot);
  if (!CompKey.IsValid() || DeviceKey.Source.IsNone()) {
    return false;
  }

  UFGPowerConnectionComponent* SourcePower =
      Session.Circuit().ResolveSubnetFeedConnector(ComponentRoot, DeviceKey);
  if (!IsValid(SourcePower)) {
    return false;
  }

  return Session.Circuit().LinkHiddenPair(DevicePlug, SourcePower);
}

bool FStructuralDeviceAttach::TryAttachSourceSupply(FStructuralGraphSession& Session,
                                                    AFGBuildable* Device,
                                                    UFGPowerConnectionComponent* DevicePlug,
                                                    int32 ComponentRoot) {
  if (!IsValid(Device) || !IsValid(DevicePlug) || ComponentRoot == INDEX_NONE) {
    return false;
  }

  UFGCircuitConnectionComponent* SourceConn =
      Session.Circuit().GetComponentSourceConnector(ComponentRoot, Device);
  UFGPowerConnectionComponent* Feed = Cast<UFGPowerConnectionComponent>(SourceConn);
  if (!IsValid(Feed)) {
    return false;
  }

  if (!Feed->IsHidden()) {
    if (AFGBuildablePowerPole* Pole = Cast<AFGBuildablePowerPole>(Feed->GetOwner())) {
      if (UFGStructuralPowerConnectionComponent* Bus =
              AStructuralPowerGraphSubsystem::FindBusConnector(Pole)) {
        Feed = Bus;
      }
    }
  }

  return Session.Circuit().LinkHiddenPair(DevicePlug, Feed);
}

void FStructuralDeviceAttach::TearDownConsumerLinks(UFGPowerConnectionComponent* DevicePlug) {
  if (!IsValid(DevicePlug)) {
    return;
  }

  TArray<UFGCircuitConnectionComponent*> HiddenLinks;
  DevicePlug->GetHiddenConnections(HiddenLinks);
  for (UFGCircuitConnectionComponent* OtherRaw : HiddenLinks) {
    if (UFGPowerConnectionComponent* Other = Cast<UFGPowerConnectionComponent>(OtherRaw)) {
      FStructuralCircuitPromotionUtil::DemoteHiddenCircuitLink(DevicePlug, Other);
    } else if (IsValid(OtherRaw)) {
      DevicePlug->RemoveHiddenConnection(OtherRaw);
      OtherRaw->RemoveHiddenConnection(DevicePlug);
    }
  }
}
