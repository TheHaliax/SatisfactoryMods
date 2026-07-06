// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Routing/EStructuralChannel.h"

class AFGBuildable;
class AFGPlayerController;

/** DR-013 raycast target for hotkey I id panel. */
class STRUCTURALPOWER_API FStructuralIdConfigTarget
{
public:
	static constexpr float TraceDistanceCm = 10000.0f;

	static bool PickFromView(AFGPlayerController* PlayerController, AFGBuildable*& OutBuildable);
	static EStructuralChannel GetTargetChannel(const AFGBuildable* Buildable);
};
