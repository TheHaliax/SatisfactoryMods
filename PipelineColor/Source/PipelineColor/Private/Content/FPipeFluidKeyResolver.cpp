// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Content/FPipeFluidKeyResolver.h"

#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildablePipeline.h"
#include "Buildables/FGBuildablePipelineAttachment.h"
#include "FGPipeConnectionComponent.h"

bool FPipeFluidKeyResolver::IsPipeColorTarget(AFGBuildable* Buildable) {
  return IsValid(Buildable) && (Buildable->IsA<AFGBuildablePipeline>() ||
                                Buildable->IsA<AFGBuildablePipelineAttachment>());
}

bool FPipeFluidKeyResolver::Supports(AFGBuildable* Buildable) const {
  return IsPipeColorTarget(Buildable);
}

void FPipeFluidKeyResolver::Resolve(AFGBuildable* Buildable,
                                    TSubclassOf<UFGItemDescriptor>& OutFluid,
                                    bool& bOutEmpty) const {
  OutFluid = nullptr;
  bOutEmpty = true;

  if (!IsValid(Buildable)) {
    return;
  }

  if (AFGBuildablePipeline* Pipe = Cast<AFGBuildablePipeline>(Buildable)) {
    OutFluid = Pipe->GetFluidDescriptor();
    if (!OutFluid) {
      // NoIndicator: ContentPct stays 0 — fluid lives on pipe connections.
      if (UFGPipeConnectionComponent* C0 = Pipe->GetPipeConnection0()) {
        OutFluid = C0->GetFluidDescriptor();
      }
      if (!OutFluid) {
        if (UFGPipeConnectionComponent* C1 = Pipe->GetPipeConnection1()) {
          OutFluid = C1->GetFluidDescriptor();
        }
      }
    }
    // Never use indicator content % (NoIndicator → false Neutral).
    bOutEmpty = !OutFluid;
    return;
  }

  if (AFGBuildablePipelineAttachment* Attachment =
          Cast<AFGBuildablePipelineAttachment>(Buildable)) {
    TArray<UFGPipeConnectionComponent*> Conns = Attachment->GetPipeConnections();
    for (UFGPipeConnectionComponent* Conn : Conns) {
      if (!IsValid(Conn)) {
        continue;
      }

      if (TSubclassOf<UFGItemDescriptor> Desc = Conn->GetFluidDescriptor()) {
        OutFluid = Desc;
        break;
      }
    }
    bOutEmpty = !OutFluid;
  }
}
