// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"

class UFGFactoryCustomizationDescriptor_PaintFinish;
class UPipelineColorRootInstanceModule;

/** Never stamp these CDOs at apply — shared Finish SIGILL. */
namespace FPCMetallicFinishPool {
constexpr int32 BucketCount = 16;

void EnsureCreated(UPipelineColorRootInstanceModule* Root);

TSubclassOf<UFGFactoryCustomizationDescriptor_PaintFinish>
Resolve(UPipelineColorRootInstanceModule* Root, float Roughness);

TSubclassOf<UFGFactoryCustomizationDescriptor_PaintFinish> GetMatteNeutral();
} // namespace FPCMetallicFinishPool
