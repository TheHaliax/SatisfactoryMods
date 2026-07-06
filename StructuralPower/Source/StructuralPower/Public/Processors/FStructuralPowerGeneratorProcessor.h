// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Core/EAttachContext.h"

class AFGBuildableGenerator;
class AStructuralPowerGraphSubsystem;

/** M7 structural generator hosts — no-op until router flag enabled. */
class STRUCTURALPOWER_API FStructuralPowerGeneratorProcessor
{
public:
	static void Process(
		AStructuralPowerGraphSubsystem& Graph,
		AFGBuildableGenerator* Generator,
		EAttachContext AttachContext);

	static void RestitchOnRoot(
		AStructuralPowerGraphSubsystem& Graph,
		int32 Root,
		EAttachContext AttachContext);

	static void TearDown(
		AStructuralPowerGraphSubsystem& Graph,
		AFGBuildableGenerator* Generator);
};
