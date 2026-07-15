// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "FGFactoryColoringTypes.h"
#include "Templates/SubclassOf.h"

class FPCSwatchSlotDispatch
{
public:
	static void RegisterHooks();

	static TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> GetActivePcDesc();
	static void SetActivePcDesc(TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Swatch);
};
