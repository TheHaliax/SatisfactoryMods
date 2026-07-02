// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Buildables/FGBuildable.h"

struct FStructuralLightweightKey
{
	TSubclassOf<AFGBuildable> BuildableClass;
	int32 Index = INDEX_NONE;

	bool IsValid() const { return BuildableClass != nullptr && Index != INDEX_NONE; }

	bool operator==(const FStructuralLightweightKey& Other) const
	{
		return BuildableClass == Other.BuildableClass && Index == Other.Index;
	}
};

inline uint32 GetTypeHash(const FStructuralLightweightKey& Key)
{
	return HashCombine(GetTypeHash(Key.BuildableClass), ::GetTypeHash(Key.Index));
}

struct FStructuralWallAnchor
{
	AFGBuildable* Actor = nullptr;
	FStructuralLightweightKey Lightweight;
	FVector WorldLocation = FVector::ZeroVector;

	bool IsValid() const { return ::IsValid(Actor) || Lightweight.IsValid(); }
};
