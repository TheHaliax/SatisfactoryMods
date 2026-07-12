// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class UWorld;

class STRUCTURALPOWER_API FStructuralGraphBootstrap
{
public:
	FStructuralGraphBootstrap() = default;

	void Bind(class FStructuralGraphSession* InSession);

	void OnWorldReady(UWorld* World);
	void PurgeSavedOutletBusMesh(UWorld* World);
	void RebuildBuildableRegistry(UWorld* World);
	void RebuildLightweightIndex(UWorld* World);

private:
	class FStructuralGraphSession* Session = nullptr;
};
