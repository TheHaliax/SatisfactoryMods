// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class AFGBuildable;
class AFGBuildableCircuitBridge;
class AFGBuildableCircuitSwitch;
class AFGBuildablePowerPole;
class AFGBuildableWire;
class AFGCircuitSubsystem;
class UFGCircuitConnectionComponent;

namespace FStructuralVanillaPowerTrace {
void RegisterHooks();

FString FormatConnector(const UFGCircuitConnectionComponent* Connector);
FString FormatBuildable(const AFGBuildable* Buildable);

void OnConnectComponentsEnter(AFGCircuitSubsystem* Subsystem, UFGCircuitConnectionComponent* First,
                              UFGCircuitConnectionComponent* Second);
void OnConnectComponentsExit(AFGCircuitSubsystem* Subsystem, UFGCircuitConnectionComponent* First,
                             UFGCircuitConnectionComponent* Second);

void OnDisconnectComponentsEnter(AFGCircuitSubsystem* Subsystem,
                                 UFGCircuitConnectionComponent* First,
                                 UFGCircuitConnectionComponent* Second);
void OnDisconnectComponentsExit(AFGCircuitSubsystem* Subsystem,
                                UFGCircuitConnectionComponent* First,
                                UFGCircuitConnectionComponent* Second);

void OnAddHiddenConnectionEnter(UFGCircuitConnectionComponent* Self,
                                UFGCircuitConnectionComponent* Other);
void OnAddHiddenConnectionExit(UFGCircuitConnectionComponent* Self,
                               UFGCircuitConnectionComponent* Other);

void OnRemoveHiddenConnectionEnter(UFGCircuitConnectionComponent* Self,
                                   UFGCircuitConnectionComponent* Other);
void OnRemoveHiddenConnectionExit(UFGCircuitConnectionComponent* Self,
                                  UFGCircuitConnectionComponent* Other);

void OnAddWireConnectionEnter(UFGCircuitConnectionComponent* Self, AFGBuildableWire* Wire);
void OnAddWireConnectionExit(UFGCircuitConnectionComponent* Self, AFGBuildableWire* Wire);

void OnRemoveWireConnectionEnter(UFGCircuitConnectionComponent* Self, AFGBuildableWire* Wire);
void OnRemoveWireConnectionExit(UFGCircuitConnectionComponent* Self, AFGBuildableWire* Wire);

void OnSetCircuitBridgesModified(AFGCircuitSubsystem* Subsystem);
void OnSetSwitchOn(AFGBuildableCircuitSwitch* Switch, bool bOn);
void OnBridgeCircuitsRebuilt(AFGBuildableCircuitBridge* Bridge);
void OnPolePowerConnectionChanged(AFGBuildablePowerPole* Pole,
                                  UFGCircuitConnectionComponent* Connection);
}
