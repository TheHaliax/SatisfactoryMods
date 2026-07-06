// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

#include "Core/EAttachContext.h"

class AFGBuildableLightSource;
class AStructuralPowerGraphSubsystem;

class STRUCTURALPOWER_API FStructuralPowerLightProcessor
{
public:
	static void Process(
		AStructuralPowerGraphSubsystem& Graph,
		AFGBuildableLightSource* Light,
		bool bLocalPromoteOnly = false);

	static void RestitchOnRoot(
		AStructuralPowerGraphSubsystem& Graph,
		int32 Root,
		EAttachContext AttachContext);

	static void TearDown(AStructuralPowerGraphSubsystem& Graph, AFGBuildableLightSource* Light);
};
