// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "FGRemoteCallObject.h"
#include "Routing/EStructuralChannel.h"
#include "UStructuralPowerRCO.generated.h"

class AFGBuildable;

UCLASS()
class STRUCTURALPOWER_API UStructuralPowerRCO : public UFGRemoteCallObject {
  GENERATED_BODY()

 public:
  virtual void
  GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

  UFUNCTION(Server, Reliable)
  void Server_SetEndpointIds(AFGBuildable* Buildable, FName Source, FName Control,
                             bool bClearSource, bool bClearControl, bool bGlobalControl = false,
                             bool bTouchGlobalControl = false);

  UFUNCTION(Server, Reliable)
  void Server_RequestComponentIdList(AFGBuildable* ContextBuildable);

  UFUNCTION(Client, Reliable)
  void Client_ReceiveComponentIdList(const FStructuralComponentIdList& List);

  UFUNCTION(Server, Reliable)
  void Server_RunBangCommand(const FString& CommandLine);

  UFUNCTION(Client, Reliable)
  void Client_SyncHoverpackTether(const FVector& Anchor, float MaxHorizontal, float MaxVertical);

  UFUNCTION(Client, Reliable)
  void Client_ClearHoverpackTether();

 private:
  UPROPERTY(Replicated, Meta = (NoAutoJson))
  bool mForceNetField_UStructuralPowerRCO = false;
};
