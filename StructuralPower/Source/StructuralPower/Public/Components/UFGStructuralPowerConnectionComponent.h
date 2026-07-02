// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "FGPowerConnectionComponent.h"
#include "UFGStructuralPowerConnectionComponent.generated.h"

/** Mod-owned hidden power connector — distinct from vanilla for interoperability. */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class STRUCTURALPOWER_API UFGStructuralPowerConnectionComponent : public UFGPowerConnectionComponent
{
	GENERATED_BODY()

public:
	UFGStructuralPowerConnectionComponent();
};
