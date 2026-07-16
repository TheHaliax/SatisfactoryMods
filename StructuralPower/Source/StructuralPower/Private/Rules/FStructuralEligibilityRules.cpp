// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Rules/FStructuralEligibilityRules.h"

#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableCircuitSwitch.h"
#include "Buildables/FGBuildableCornerWall.h"
#include "Buildables/FGBuildableDockingStation.h"
#include "Buildables/FGBuildableDroneStation.h"
#include "Buildables/FGBuildableElevator.h"
#include "Buildables/FGBuildableFactory.h"
#include "Buildables/FGBuildableFactoryBuilding.h"
#include "Buildables/FGBuildableFactorySimpleProducer.h"
#include "Buildables/FGBuildableFoundation.h"
#include "Buildables/FGBuildableGenerator.h"
#include "Buildables/FGBuildableJumppad.h"
#include "Buildables/FGBuildableLightSource.h"
#include "Buildables/FGBuildableLightsControlPanel.h"
#include "Buildables/FGBuildableManufacturer.h"
#include "Buildables/FGBuildablePipeHyper.h"
#include "Buildables/FGBuildablePipeHyperAttachment.h"
#include "Buildables/FGBuildablePipeReservoir.h"
#include "Buildables/FGBuildablePipeline.h"
#include "Buildables/FGBuildablePipelineAttachment.h"
#include "Buildables/FGBuildablePipelinePump.h"
#include "Buildables/FGBuildablePortalBase.h"
#include "UObject/SoftObjectPath.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "Buildables/FGBuildablePowerStorage.h"
#include "Buildables/FGBuildableRadarTower.h"
#include "Buildables/FGBuildableRailroadStation.h"
#include "Buildables/FGBuildableRamp.h"
#include "Buildables/FGBuildableResourceExtractorBase.h"
#include "Buildables/FGBuildableResourceSink.h"
#include "Buildables/FGBuildableStair.h"
#include "Buildables/FGBuildableTrainPlatform.h"
#include "Buildables/FGBuildableWalkway.h"
#include "Buildables/FGBuildableWall.h"
#include "Buildables/FGBuildableWindTurbine.h"
#include "Buildables/FGPipeHyperStart.h"
#include "FGBuildableBeam.h"
#include "FGBuildablePillar.h"
#include "FGBuildablePowerBooster.h"
#include "FGPowerInfoComponent.h"
#include "Routing/EStructuralChannel.h"

bool FStructuralEligibilityRules::IsBusMember(const AFGBuildable* Buildable) {
  if (!IsValid(Buildable) || Buildable->IsA<AFGBuildableFactory>()) {
    return false;
  }

  if (Buildable->IsA<AFGBuildablePowerPole>()) {
    return false;
  }

  TInlineComponentArray<UFGPowerInfoComponent*> PowerInfos;
  const_cast<AFGBuildable*>(Buildable)->GetComponents(PowerInfos);
  if (PowerInfos.Num() > 0) {
    return false;
  }

  // Mod structural packs often inherit factory building without UFGPowerInfoComponent.
  return Buildable->IsA<AFGBuildableFactoryBuilding>() ||
         Buildable->IsA<AFGBuildableFoundation>() || Buildable->IsA<AFGBuildableRamp>() ||
         Buildable->IsA<AFGBuildableStair>() || Buildable->IsA<AFGBuildableWalkway>() ||
         Buildable->IsA<AFGBuildableWall>() || Buildable->IsA<AFGBuildableCornerWall>() ||
         Buildable->IsA<AFGBuildableBeam>() || Buildable->IsA<AFGBuildablePillar>();
}

bool FStructuralEligibilityRules::IsPowerBridgeSwitch(const AFGBuildable* Buildable) {
  return IsValid(Buildable) && Buildable->IsA<AFGBuildableCircuitSwitch>();
}

bool FStructuralEligibilityRules::IsStructuralLightConsumer(const AFGBuildable* Buildable) {
  return IsValid(Buildable) && Buildable->IsA<AFGBuildableLightSource>() &&
         !Buildable->IsA<AFGBuildableLightsControlPanel>();
}

bool FStructuralEligibilityRules::IsStructuralGenerator(const AFGBuildable* Buildable) {
  if (!IsValid(Buildable)) {
    return false;
  }

  // Wind / booster are AFGBuildableFactory — not AFGBuildableGenerator (header-verified).
  return Buildable->IsA<AFGBuildableGenerator>() || Buildable->IsA<AFGBuildableWindTurbine>() ||
         Buildable->IsA<AFGBuildablePowerBooster>();
}

bool FStructuralEligibilityRules::IsStructuralExtractor(const AFGBuildable* Buildable) {
  return IsValid(Buildable) && Buildable->IsA<AFGBuildableResourceExtractorBase>();
}

bool FStructuralEligibilityRules::IsStructuralManufacturer(const AFGBuildable* Buildable) {
  if (!IsValid(Buildable) || Buildable->IsA<AFGBuildableFactorySimpleProducer>()) {
    return false;
  }

  return Buildable->IsA<AFGBuildableManufacturer>() || Buildable->IsA<AFGBuildableRadarTower>() ||
         Buildable->IsA<AFGBuildableResourceSink>();
}

bool FStructuralEligibilityRules::IsStructuralTransport(const AFGBuildable* Buildable) {
  if (!IsValid(Buildable)) {
    return false;
  }

  return Buildable->IsA<AFGBuildableTrainPlatform>() ||
         Buildable->IsA<AFGBuildableRailroadStation>() ||
         Buildable->IsA<AFGBuildableDockingStation>() ||
         Buildable->IsA<AFGBuildableDroneStation>() || Buildable->IsA<AFGBuildableJumppad>() ||
         Buildable->IsA<AFGBuildablePortalBase>() || Buildable->IsA<AFGBuildableElevator>() ||
         Buildable->IsA<AFGPipeHyperStart>();
}

bool FStructuralEligibilityRules::IsStructuralPipelinePump(const AFGBuildable* Buildable) {
  return IsValid(Buildable) && Buildable->IsA<AFGBuildablePipelinePump>();
}

bool FStructuralEligibilityRules::IsFluidPipeConductor(const AFGBuildable* Buildable) {
  if (!IsValid(Buildable) || Buildable->IsA<AFGBuildablePipeHyper>() ||
      Buildable->IsA<AFGBuildablePipeHyperAttachment>()) {
    return false;
  }

  return Buildable->IsA<AFGBuildablePipeline>() ||
         Buildable->IsA<AFGBuildablePipelineAttachment>() ||
         Buildable->IsA<AFGBuildablePipeReservoir>();
}

bool FStructuralEligibilityRules::IsFluidPipeSupport(const AFGBuildable* Buildable) {
  if (!IsValid(Buildable) || IsFluidPipeConductor(Buildable)) {
    return false;
  }

  static TArray<TSubclassOf<AFGBuildable>> Parents;
  static bool bReady = false;
  if (!bReady) {
    bReady = true;
    const TCHAR* SoftPaths[] = {
        TEXT("/Game/FactoryGame/Buildable/Factory/PipelineSupport/"
             "Build_PipelineSupport.Build_PipelineSupport_C"),
        TEXT("/Game/FactoryGame/Buildable/Factory/PipelineSupport/"
             "Build_PipeSupportStackable.Build_PipeSupportStackable_C"),
        TEXT("/Game/FactoryGame/Buildable/Factory/PipelineSupportWall/"
             "Build_PipelineSupportWall.Build_PipelineSupportWall_C"),
        TEXT("/Game/FactoryGame/Buildable/Factory/PipelineSupportWallHole/"
             "Build_PipelineSupportWallHole.Build_PipelineSupportWallHole_C"),
    };
    for (const TCHAR* SoftPath : SoftPaths) {
      if (TSubclassOf<AFGBuildable> Cls = FSoftClassPath(SoftPath).TryLoadClass<AFGBuildable>()) {
        Parents.Add(Cls);
      }
    }
  }

  for (const TSubclassOf<AFGBuildable>& Parent : Parents) {
    if (Parent && Buildable->IsA(Parent)) {
      return true;
    }
  }
  return false;
}

bool FStructuralEligibilityRules::IsIdConfigTarget(const AFGBuildable* Buildable) {
  if (!IsValid(Buildable)) {
    return false;
  }

  if (IsStructuralLightConsumer(Buildable)) {
    return true;
  }

  if (IsPowerBridgeSwitch(Buildable)) {
    return true;
  }

  if (Buildable->IsA<AFGBuildableLightsControlPanel>()) {
    return true;
  }

  return IsStructuralGenerator(Buildable) || IsStructuralExtractor(Buildable) ||
         IsStructuralManufacturer(Buildable) || IsStructuralTransport(Buildable) ||
         IsStructuralPipelinePump(Buildable);
}

bool FStructuralEligibilityRules::IsPowerBridgePole(const AFGBuildable* Buildable) {
  const AFGBuildablePowerPole* Pole = Cast<AFGBuildablePowerPole>(Buildable);
  if (!Pole) {
    return false;
  }

  switch (Pole->GetPowerPoleType()) {
    case EPowerPoleType::PPT_WALL:
    case EPowerPoleType::PPT_WALL_DOUBLE:
    case EPowerPoleType::PPT_POLE:
    case EPowerPoleType::PPT_TOWER:
      return true;
    default:
      return false;
  }
}

bool FStructuralEligibilityRules::IsPowerStorage(const AFGBuildable* Buildable) {
  return IsValid(Buildable) && Buildable->IsA<AFGBuildablePowerStorage>();
}

bool FStructuralEligibilityRules::PrefersFoundationMount(const AFGBuildable* Buildable) {
  return IsStructuralGenerator(Buildable) || IsPowerStorage(Buildable) ||
         IsStructuralExtractor(Buildable) || IsStructuralManufacturer(Buildable) ||
         IsStructuralTransport(Buildable) || IsStructuralPipelinePump(Buildable);
}

bool FStructuralEligibilityRules::IsValidOutletParent(const AFGBuildable* Parent) {
  return IsBusMember(Parent);
}

EStructuralChannel FStructuralEligibilityRules::ClassifyBuildable(const AFGBuildable* Buildable) {
  if (!IsValid(Buildable)) {
    return EStructuralChannel::Structure;
  }

  if (Buildable->IsA<AFGBuildableCircuitSwitch>()) {
    return EStructuralChannel::Switch;
  }

  if (Buildable->IsA<AFGBuildableLightsControlPanel>() ||
      Buildable->IsA<AFGBuildableLightSource>()) {
    return EStructuralChannel::Light;
  }

  if (IsStructuralGenerator(Buildable)) {
    return EStructuralChannel::Generator;
  }

  if (IsStructuralExtractor(Buildable)) {
    return EStructuralChannel::Extractor;
  }

  if (IsStructuralManufacturer(Buildable)) {
    return EStructuralChannel::Manufacturer;
  }

  if (IsStructuralTransport(Buildable)) {
    return EStructuralChannel::Transport;
  }

  if (IsStructuralPipelinePump(Buildable)) {
    return EStructuralChannel::Misc;
  }

  return EStructuralChannel::Structure;
}
