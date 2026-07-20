// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "FGFactoryColoringTypes.h"
#include "FGRemoteCallObject.h"
#include "UPCChatRCO.generated.h"

UCLASS()
class PIPELINECOLOR_API UPCChatRCO : public UFGRemoteCallObject {
  GENERATED_BODY()

 public:
  virtual void
  GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

  UFUNCTION(Server, Reliable)
  void Server_RunBangCommand(const FString& CommandLine);

  UFUNCTION(Server, Reliable)
  void Server_SetActivePcSwatch(TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Swatch);

  UFUNCTION(Server, Reliable)
  void Server_SetPcSwatchColors(TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Swatch,
                                FFactoryCustomizationColorSlot ColorData);

 private:
  UPROPERTY(Replicated, Meta = (NoAutoJson))
  bool mForceNetField_UPCChatRCO = false;
};
