// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "FGFactoryColoringTypes.h"
#include "UPCFinishDescs.generated.h"

UCLASS()
class PIPELINECOLOR_API UPCFinish_MetallicColor : public UFGFactoryCustomizationDescriptor_PaintFinish
{
	GENERATED_BODY()

public:
	UPCFinish_MetallicColor();

	static void EnsureIconLoaded();
};
