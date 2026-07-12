// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Routing/EStructuralChannel.h"

class UWorld;

struct STRUCTURALPOWER_API FStructuralGroupToggleDef
{
	FName ConfigKey;
	EStructuralChannel Channel = EStructuralChannel::Structure;
	bool (*IsEnabled)() = nullptr;
	void (*RequestReconcile)(UWorld*) = nullptr;
};

class STRUCTURALPOWER_API FStructuralGroupToggleRegistry
{
public:
	static FStructuralGroupToggleRegistry& Get();

	void Initialize();

	bool IsChannelRoutingEnabled(EStructuralChannel Channel) const;
	const FStructuralGroupToggleDef* FindByKey(FName ConfigKey) const;
	void ForEach(const TFunctionRef<void(const FStructuralGroupToggleDef&)> Visitor) const;

private:
	TArray<FStructuralGroupToggleDef> Definitions;
	bool bInitialized = false;
};
