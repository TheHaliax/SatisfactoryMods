// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Diagnostics/FStructuralVanillaPowerTrace.h"

#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableCircuitBridge.h"
#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "Buildables/FGBuildableWire.h"
#include "Config/FStructuralPowerModConfig.h"
#include "Diagnostics/FStructuralPowerTraceUtil.h"
#include "FGCircuitConnectionComponent.h"
#include "FGCircuitSubsystem.h"
#include "HAL/PlatformTime.h"
#include "Patching/NativeHookManager.h"
#include "StructuralPowerLog.h"

namespace {
struct FVanillaHookFrame {
  const TCHAR* Op = nullptr;
  uint64 StartCycles = 0;
  FString Detail;
};

thread_local TArray<FVanillaHookFrame> GVanillaHookStack;

bool ShouldLog() {
  return FStructuralPowerModConfig::IsExtendedDebugEnabled();
}

void PushHook(const TCHAR* Op, const FString& Detail) {
  if (!ShouldLog() || !Op) {
    return;
  }

  FVanillaHookFrame Frame;
  Frame.Op = Op;
  Frame.Detail = Detail;
  Frame.StartCycles = FPlatformTime::Cycles64();
  GVanillaHookStack.Add(Frame);

  UE_LOG(LogStructuralPower, Log, TEXT("[HALSP] vanilla> %s frame=%d depth=%d %s"), Op,
         FStructuralPowerTraceUtil::GetFrameNumber(), GVanillaHookStack.Num() - 1, *Detail);
}

void PopHook() {
  if (!ShouldLog() || GVanillaHookStack.Num() == 0) {
    return;
  }

  const FVanillaHookFrame Frame = GVanillaHookStack.Last();
  GVanillaHookStack.Pop();

  const double ElapsedMs =
      FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - Frame.StartCycles);

  UE_LOG(LogStructuralPower, Log, TEXT("[HALSP] vanilla< %s ms=%.3f frame=%d depth=%d %s"),
         Frame.Op ? Frame.Op : TEXT("?"), ElapsedMs, FStructuralPowerTraceUtil::GetFrameNumber(),
         GVanillaHookStack.Num(), *Frame.Detail);
}

FString PairDetail(const TCHAR* LabelA, const UFGCircuitConnectionComponent* A, const TCHAR* LabelB,
                   const UFGCircuitConnectionComponent* B) {
  return FString::Printf(TEXT("%s={%s} %s={%s}"), LabelA,
                         *FStructuralVanillaPowerTrace::FormatConnector(A), LabelB,
                         *FStructuralVanillaPowerTrace::FormatConnector(B));
}
}  // namespace

FString FStructuralVanillaPowerTrace::FormatConnector(
    const UFGCircuitConnectionComponent* Connector) {
  if (!IsValid(Connector)) {
    return TEXT("null");
  }

  const AActor* Owner = Connector->GetOwner();
  return FString::Printf(TEXT("%s.%s hidden=%d circuit=%d wires=%d hiddenN=%d"),
                         IsValid(Owner) ? *Owner->GetName() : TEXT("?"), *Connector->GetName(),
                         Connector->IsHidden() ? 1 : 0, Connector->GetCircuitID(),
                         Connector->GetNumConnections(), Connector->GetNumHiddenConnections());
}

FString FStructuralVanillaPowerTrace::FormatBuildable(const AFGBuildable* Buildable) {
  if (!IsValid(Buildable)) {
    return TEXT("null");
  }

  return FString::Printf(TEXT("%s class=%s"), *Buildable->GetName(),
                         *Buildable->GetClass()->GetName());
}

void FStructuralVanillaPowerTrace::OnConnectComponentsEnter(AFGCircuitSubsystem* Subsystem,
                                                            UFGCircuitConnectionComponent* First,
                                                            UFGCircuitConnectionComponent* Second) {
  if (!ShouldLog()) {
    return;
  }

  PushHook(TEXT("ConnectComponents"),
           FString::Printf(TEXT("subsystem=%p %s"), Subsystem,
                           *PairDetail(TEXT("A"), First, TEXT("B"), Second)));
}

void FStructuralVanillaPowerTrace::OnConnectComponentsExit(AFGCircuitSubsystem* /*Subsystem*/,
                                                           UFGCircuitConnectionComponent* First,
                                                           UFGCircuitConnectionComponent* Second) {
  if (!ShouldLog()) {
    return;
  }

  if (GVanillaHookStack.Num() > 0) {
    GVanillaHookStack.Last().Detail = PairDetail(TEXT("A"), First, TEXT("B"), Second);
  }

  PopHook();
}

void FStructuralVanillaPowerTrace::OnDisconnectComponentsEnter(
    AFGCircuitSubsystem* Subsystem, UFGCircuitConnectionComponent* First,
    UFGCircuitConnectionComponent* Second) {
  if (!ShouldLog()) {
    return;
  }

  PushHook(TEXT("DisconnectComponents"),
           FString::Printf(TEXT("subsystem=%p %s"), Subsystem,
                           *PairDetail(TEXT("A"), First, TEXT("B"), Second)));
}

void FStructuralVanillaPowerTrace::OnDisconnectComponentsExit(
    AFGCircuitSubsystem* /*Subsystem*/, UFGCircuitConnectionComponent* First,
    UFGCircuitConnectionComponent* Second) {
  if (!ShouldLog()) {
    return;
  }

  if (GVanillaHookStack.Num() > 0) {
    GVanillaHookStack.Last().Detail = PairDetail(TEXT("A"), First, TEXT("B"), Second);
  }

  PopHook();
}

void FStructuralVanillaPowerTrace::OnAddHiddenConnectionEnter(
    UFGCircuitConnectionComponent* Self, UFGCircuitConnectionComponent* Other) {
  if (!ShouldLog()) {
    return;
  }

  PushHook(TEXT("AddHiddenConnection"), PairDetail(TEXT("self"), Self, TEXT("other"), Other));
}

void FStructuralVanillaPowerTrace::OnAddHiddenConnectionExit(UFGCircuitConnectionComponent* Self,
                                                             UFGCircuitConnectionComponent* Other) {
  if (!ShouldLog()) {
    return;
  }

  if (GVanillaHookStack.Num() > 0) {
    GVanillaHookStack.Last().Detail = PairDetail(TEXT("self"), Self, TEXT("other"), Other);
  }

  PopHook();
}

void FStructuralVanillaPowerTrace::OnRemoveHiddenConnectionEnter(
    UFGCircuitConnectionComponent* Self, UFGCircuitConnectionComponent* Other) {
  if (!ShouldLog()) {
    return;
  }

  PushHook(TEXT("RemoveHiddenConnection"), PairDetail(TEXT("self"), Self, TEXT("other"), Other));
}

void FStructuralVanillaPowerTrace::OnRemoveHiddenConnectionExit(
    UFGCircuitConnectionComponent* Self, UFGCircuitConnectionComponent* Other) {
  if (!ShouldLog()) {
    return;
  }

  if (GVanillaHookStack.Num() > 0) {
    GVanillaHookStack.Last().Detail = PairDetail(TEXT("self"), Self, TEXT("other"), Other);
  }

  PopHook();
}

void FStructuralVanillaPowerTrace::OnAddWireConnectionEnter(UFGCircuitConnectionComponent* Self,
                                                            AFGBuildableWire* Wire) {
  if (!ShouldLog()) {
    return;
  }

  PushHook(TEXT("AddConnection"), FString::Printf(TEXT("conn={%s} wire=%s"), *FormatConnector(Self),
                                                  IsValid(Wire) ? *Wire->GetName() : TEXT("null")));
}

void FStructuralVanillaPowerTrace::OnAddWireConnectionExit(UFGCircuitConnectionComponent* Self,
                                                           AFGBuildableWire* Wire) {
  if (!ShouldLog()) {
    return;
  }

  if (GVanillaHookStack.Num() > 0) {
    GVanillaHookStack.Last().Detail =
        FString::Printf(TEXT("conn={%s} wire=%s circuit=%d"), *FormatConnector(Self),
                        IsValid(Wire) ? *Wire->GetName() : TEXT("null"),
                        IsValid(Self) ? Self->GetCircuitID() : INDEX_NONE);
  }

  PopHook();
}

void FStructuralVanillaPowerTrace::OnRemoveWireConnectionEnter(UFGCircuitConnectionComponent* Self,
                                                               AFGBuildableWire* Wire) {
  if (!ShouldLog()) {
    return;
  }

  PushHook(TEXT("RemoveConnection"),
           FString::Printf(TEXT("conn={%s} wire=%s"), *FormatConnector(Self),
                           IsValid(Wire) ? *Wire->GetName() : TEXT("null")));
}

void FStructuralVanillaPowerTrace::OnRemoveWireConnectionExit(UFGCircuitConnectionComponent* Self,
                                                              AFGBuildableWire* Wire) {
  if (!ShouldLog()) {
    return;
  }

  if (GVanillaHookStack.Num() > 0) {
    GVanillaHookStack.Last().Detail =
        FString::Printf(TEXT("conn={%s} wire=%s circuit=%d"), *FormatConnector(Self),
                        IsValid(Wire) ? *Wire->GetName() : TEXT("null"),
                        IsValid(Self) ? Self->GetCircuitID() : INDEX_NONE);
  }

  PopHook();
}

void FStructuralVanillaPowerTrace::OnSetCircuitBridgesModified(AFGCircuitSubsystem* Subsystem) {
  if (!ShouldLog()) {
    return;
  }

  UE_LOG(LogStructuralPower, Log,
         TEXT("[HALSP] vanilla evt SetCircuitBridgesModified subsystem=%p frame=%d"), Subsystem,
         FStructuralPowerTraceUtil::GetFrameNumber());
}

void FStructuralVanillaPowerTrace::OnSetSwitchOn(AFGBuildableCircuitSwitch* Switch, bool bOn) {
  if (!ShouldLog()) {
    return;
  }

  UE_LOG(LogStructuralPower, Log,
         TEXT("[HALSP] vanilla evt SetSwitchOn %s on=%d bridgeActive=%d frame=%d"),
         *FormatBuildable(Switch), bOn ? 1 : 0, IsValid(Switch) && Switch->IsBridgeActive() ? 1 : 0,
         FStructuralPowerTraceUtil::GetFrameNumber());
}

void FStructuralVanillaPowerTrace::OnBridgeCircuitsRebuilt(AFGBuildableCircuitBridge* Bridge) {
  if (!ShouldLog()) {
    return;
  }

  UE_LOG(LogStructuralPower, Log, TEXT("[HALSP] vanilla evt OnCircuitsRebuilt %s frame=%d"),
         *FormatBuildable(Bridge), FStructuralPowerTraceUtil::GetFrameNumber());
}

void FStructuralVanillaPowerTrace::OnPolePowerConnectionChanged(
    AFGBuildablePowerPole* Pole, UFGCircuitConnectionComponent* Connection) {
  if (!ShouldLog()) {
    return;
  }

  UE_LOG(LogStructuralPower, Log,
         TEXT("[HALSP] vanilla evt OnPowerConnectionChanged pole=%s conn={%s} frame=%d"),
         IsValid(Pole) ? *Pole->GetName() : TEXT("null"), *FormatConnector(Connection),
         FStructuralPowerTraceUtil::GetFrameNumber());
}

void FStructuralVanillaPowerTrace::RegisterHooks() {
  SUBSCRIBE_METHOD(AFGCircuitSubsystem::ConnectComponents,
                   [](auto& /*Scope*/, AFGCircuitSubsystem* Subsystem,
                      UFGCircuitConnectionComponent* First, UFGCircuitConnectionComponent* Second) {
                     OnConnectComponentsEnter(Subsystem, First, Second);
                   });

  SUBSCRIBE_METHOD_AFTER(AFGCircuitSubsystem::ConnectComponents,
                         [](AFGCircuitSubsystem* Subsystem, UFGCircuitConnectionComponent* First,
                            UFGCircuitConnectionComponent* Second) {
                           OnConnectComponentsExit(Subsystem, First, Second);
                         });

  SUBSCRIBE_METHOD(AFGCircuitSubsystem::DisconnectComponents,
                   [](auto& /*Scope*/, AFGCircuitSubsystem* Subsystem,
                      UFGCircuitConnectionComponent* First, UFGCircuitConnectionComponent* Second) {
                     OnDisconnectComponentsEnter(Subsystem, First, Second);
                   });

  SUBSCRIBE_METHOD_AFTER(AFGCircuitSubsystem::DisconnectComponents,
                         [](AFGCircuitSubsystem* Subsystem, UFGCircuitConnectionComponent* First,
                            UFGCircuitConnectionComponent* Second) {
                           OnDisconnectComponentsExit(Subsystem, First, Second);
                         });

  SUBSCRIBE_METHOD_AFTER(
      AFGCircuitSubsystem::SetCircuitBridgesModified,
      [](AFGCircuitSubsystem* Subsystem) { OnSetCircuitBridgesModified(Subsystem); });

  SUBSCRIBE_METHOD(
      UFGCircuitConnectionComponent::AddHiddenConnection,
      [](auto& /*Scope*/, UFGCircuitConnectionComponent* Self,
         UFGCircuitConnectionComponent* Other) { OnAddHiddenConnectionEnter(Self, Other); });

  SUBSCRIBE_METHOD_AFTER(
      UFGCircuitConnectionComponent::AddHiddenConnection,
      [](UFGCircuitConnectionComponent* Self, UFGCircuitConnectionComponent* Other) {
        OnAddHiddenConnectionExit(Self, Other);
      });

  SUBSCRIBE_METHOD(
      UFGCircuitConnectionComponent::RemoveHiddenConnection,
      [](auto& /*Scope*/, UFGCircuitConnectionComponent* Self,
         UFGCircuitConnectionComponent* Other) { OnRemoveHiddenConnectionEnter(Self, Other); });

  SUBSCRIBE_METHOD_AFTER(
      UFGCircuitConnectionComponent::RemoveHiddenConnection,
      [](UFGCircuitConnectionComponent* Self, UFGCircuitConnectionComponent* Other) {
        OnRemoveHiddenConnectionExit(Self, Other);
      });

  SUBSCRIBE_METHOD(UFGCircuitConnectionComponent::AddConnection,
                   [](auto& /*Scope*/, UFGCircuitConnectionComponent* Self,
                      AFGBuildableWire* Wire) { OnAddWireConnectionEnter(Self, Wire); });

  SUBSCRIBE_METHOD_AFTER(UFGCircuitConnectionComponent::AddConnection,
                         [](UFGCircuitConnectionComponent* Self, AFGBuildableWire* Wire) {
                           OnAddWireConnectionExit(Self, Wire);
                         });

  SUBSCRIBE_METHOD(UFGCircuitConnectionComponent::RemoveConnection,
                   [](auto& /*Scope*/, UFGCircuitConnectionComponent* Self,
                      AFGBuildableWire* Wire) { OnRemoveWireConnectionEnter(Self, Wire); });

  SUBSCRIBE_METHOD_AFTER(UFGCircuitConnectionComponent::RemoveConnection,
                         [](UFGCircuitConnectionComponent* Self, AFGBuildableWire* Wire) {
                           OnRemoveWireConnectionExit(Self, Wire);
                         });

  SUBSCRIBE_METHOD_AFTER(
      AFGBuildableCircuitSwitch::SetSwitchOn,
      [](AFGBuildableCircuitSwitch* Switch, bool bOn) { OnSetSwitchOn(Switch, bOn); });

  SUBSCRIBE_METHOD_VIRTUAL_AFTER(
      AFGBuildableCircuitBridge::OnCircuitsRebuilt, GetMutableDefault<AFGBuildableCircuitBridge>(),
      [](AFGBuildableCircuitBridge* Bridge) { OnBridgeCircuitsRebuilt(Bridge); });

  SUBSCRIBE_METHOD_VIRTUAL_AFTER(
      AFGBuildablePowerPole::OnPowerConnectionChanged, GetMutableDefault<AFGBuildablePowerPole>(),
      [](AFGBuildablePowerPole* Pole, UFGCircuitConnectionComponent* Connection) {
        OnPolePowerConnectionChanged(Pole, Connection);
      });
}
