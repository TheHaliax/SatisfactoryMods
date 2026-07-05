// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "FGRemoteCallObject.h"
#include "UStructuralPowerRCO.generated.h"

class AFGBuildable;

/** DR-009 — client UI → authority Id override on graph subsystem save registry. */
UCLASS()
class STRUCTURALPOWER_API UStructuralPowerRCO : public UFGRemoteCallObject
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(Server, Reliable)
	void Server_SetPlayerOverrideId(AFGBuildable* Buildable, FName PlayerOverrideId);

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
