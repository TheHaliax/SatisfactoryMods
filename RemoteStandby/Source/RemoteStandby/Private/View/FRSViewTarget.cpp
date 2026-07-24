// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "View/FRSViewTarget.h"

#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableFactory.h"
#include "Camera/PlayerCameraManager.h"
#include "FGPlayerController.h"

bool FRSViewTarget::PickFactory(AFGPlayerController* PlayerController,
                                AFGBuildableFactory*& OutFactory) {
  OutFactory = nullptr;
  if (!IsValid(PlayerController)) {
    return false;
  }

  APlayerCameraManager* Camera = PlayerController->PlayerCameraManager;
  if (!IsValid(Camera)) {
    return false;
  }

  const FVector Start = Camera->GetCameraLocation();
  const FVector End = Start + Camera->GetCameraRotation().Vector() * TraceDistanceCm;

  FHitResult Hit;
  FCollisionQueryParams Params(SCENE_QUERY_STAT(RemoteStandbyViewTrace),
                               /*bTraceComplex=*/true);
  Params.AddIgnoredActor(PlayerController->GetPawn());

  UWorld* World = PlayerController->GetWorld();
  if (!IsValid(World) ||
      !World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params)) {
    return false;
  }

  AActor* HitActor = Hit.GetActor();
  while (IsValid(HitActor)) {
    if (AFGBuildableFactory* Factory = Cast<AFGBuildableFactory>(HitActor)) {
      OutFactory = Factory;
      return true;
    }
    if (!Cast<AFGBuildable>(HitActor)) {
      break;
    }
    HitActor = HitActor->GetAttachParentActor();
  }

  return false;
}
