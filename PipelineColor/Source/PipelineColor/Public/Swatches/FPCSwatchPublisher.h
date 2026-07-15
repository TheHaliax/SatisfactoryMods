// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class UWorld;

class FPCSwatchPublisher
{
public:
	static void RegisterForceRecipeHook();
	static void PublishForWorld(UWorld* World);
};
