// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class AFGBuildableControlPanelHost;
class UFGCircuitConnectionComponent;
class UFGPowerConnectionComponent;

struct FStructuralPanelPorts
{
	UFGCircuitConnectionComponent* Downstream = nullptr;
	UFGCircuitConnectionComponent* Input = nullptr;
};

class STRUCTURALPOWER_API FStructuralPanelPortResolver
{
public:
	static bool Resolve(AFGBuildableControlPanelHost* Panel, FStructuralPanelPorts& Out);
	static UFGPowerConnectionComponent* AsPowerConnection(UFGCircuitConnectionComponent* Conn);
};
