// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Network/URSStandbyRCO.h"

#include "Buildables/FGBuildableFactory.h"
#include "Camera/PlayerCameraManager.h"
#include "FGPlayerController.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "RemoteStandbyLog.h"
#include "View/FRSViewTarget.h"

void URSStandbyRCO::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const {
  Super::GetLifetimeReplicatedProps(OutLifetimeProps);
  DOREPLIFETIME(URSStandbyRCO, mForceNetField_URSStandbyRCO);
}

void URSStandbyRCO::Server_ToggleStandby_Implementation(AFGBuildableFactory* Factory) {
  if (!IsValid(Factory) || !Factory->HasAuthority()) {
    return;
  }

  AFGPlayerController* PC = GetOwnerPlayerController();
  if (!IsValid(PC)) {
    return;
  }

  FVector Origin = FVector::ZeroVector;
  if (APawn* Pawn = PC->GetPawn()) {
    Origin = Pawn->GetActorLocation();
  } else if (IsValid(PC->PlayerCameraManager)) {
    Origin = PC->PlayerCameraManager->GetCameraLocation();
  } else {
    return;
  }

  const float MaxDistSq = FMath::Square(FRSViewTarget::TraceDistanceCm);
  if (FVector::DistSquared(Origin, Factory->GetActorLocation()) > MaxDistSq) {
    UE_LOG(LogRemoteStandby, Warning, TEXT("%s RCO ToggleStandby rejected — range %s"),
           REMOTESTANDBY_LOG_PREFIX, *Factory->GetName());
    return;
  }

  const bool bPause = !Factory->IsProductionPaused();
  Factory->SetIsProductionPaused(bPause);
  UE_LOG(LogRemoteStandby, Display, TEXT("%s standby %s on %s"), REMOTESTANDBY_LOG_PREFIX,
         bPause ? TEXT("on") : TEXT("off"), *Factory->GetName());
}
