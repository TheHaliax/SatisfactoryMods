// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "FGRemoteCallObject.h"
#include "URSStandbyRCO.generated.h"

class AFGBuildableFactory;

UCLASS()
class REMOTESTANDBY_API URSStandbyRCO : public UFGRemoteCallObject {
  GENERATED_BODY()

 public:
  virtual void
  GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

  UFUNCTION(Server, Reliable)
  void Server_ToggleStandby(AFGBuildableFactory* Factory);

 private:
  UPROPERTY(Replicated, Meta = (NoAutoJson))
  bool mForceNetField_URSStandbyRCO = false;
};
