// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Observation/FFluidAppearanceObserver.h"

#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildablePipeline.h"
#include "Buildables/FGBuildablePipelineAttachment.h"
#include "Buildables/FGBuildablePipelinePump.h"
#include "Content/FPipeFluidKeyResolver.h"
#include "FGFluidIntegrantInterface.h"
#include "FGPipeNetwork.h"
#include "Patching/NativeHookManager.h"
#include "PipelineColorLog.h"
#include "Session/UPCWorldSubsystem.h"

namespace {
void EnqueueBuildable(AFGBuildable* Buildable) {
  if (!IsValid(Buildable) || !Buildable->HasAuthority()) {
    return;
  }

  if (UPCWorldSubsystem* Sys = UPCWorldSubsystem::Get(Buildable->GetWorld())) {
    Sys->Enqueue(Buildable);
  }
}

void EnqueueIntegrantActor(IFGFluidIntegrantInterface* Integrant) {
  if (!Integrant) {
    return;
  }

  if (AFGBuildable* Buildable = Cast<AFGBuildable>(Integrant)) {
    EnqueueBuildable(Buildable);
  }
}

void EnqueueNetworkFirstIntegrant(AFGPipeNetwork* Network) {
  if (!IsValid(Network) || !Network->HasAuthority()) {
    return;
  }

  if (IFGFluidIntegrantInterface* First = Network->GetFirstFluidIntegrant()) {
    EnqueueIntegrantActor(First);
  }
}
}  // namespace

void FFluidAppearanceObserver::EnqueueFromWorld(UWorld* World, AFGBuildable* Buildable) {
  if (UPCWorldSubsystem* Sys = UPCWorldSubsystem::Get(World)) {
    Sys->Enqueue(Buildable);
  }
}

void FFluidAppearanceObserver::RegisterHooks() {
#if WITH_EDITOR
  UE_LOG(LogPipelineColor, Log, TEXT("%s skip hooks in editor"), PIPELINECOLOR_LOG_PREFIX);
  return;
#else
  SUBSCRIBE_METHOD_VIRTUAL_AFTER(AFGBuildablePipeline::OnFluidDescriptorSet,
                                 GetMutableDefault<AFGBuildablePipeline>(),
                                 [](AFGBuildablePipeline* Pipe) { EnqueueBuildable(Pipe); });

  SUBSCRIBE_METHOD_VIRTUAL_AFTER(AFGBuildablePipelinePump::OnFluidDescriptorSet,
                                 GetMutableDefault<AFGBuildablePipelinePump>(),
                                 [](AFGBuildablePipelinePump* Pump) { EnqueueBuildable(Pump); });

  SUBSCRIBE_METHOD_AFTER(AFGPipeNetwork::FlushNetwork,
                         [](AFGPipeNetwork* Network) { EnqueueNetworkFirstIntegrant(Network); });

  SUBSCRIBE_METHOD_AFTER(AFGPipeNetwork::TrySetFluidDescriptor,
                         [](AFGPipeNetwork* Network, TSubclassOf<UFGItemDescriptor> /*Desc*/) {
                           EnqueueNetworkFirstIntegrant(Network);
                         });

  UE_LOG(LogPipelineColor, Log, TEXT("%s fluid hooks registered"), PIPELINECOLOR_LOG_PREFIX);
#endif
}
