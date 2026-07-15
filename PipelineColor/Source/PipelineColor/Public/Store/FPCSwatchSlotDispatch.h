// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "FGFactoryColoringTypes.h"
#include "Templates/SubclassOf.h"

class AFGPlayerController;

class FPCSwatchSlotDispatch
{
public:
	static void RegisterHooks();

	static TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> GetActivePcDesc(
		AFGPlayerController* PlayerController);
	static void SetActivePcDesc(
		AFGPlayerController* PlayerController,
		TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Swatch);

	static AFGPlayerController* ResolvePlayerController(UObject* WorldContext);
};
