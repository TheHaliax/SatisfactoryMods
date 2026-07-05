// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Core/FStructuralNodeId.h"
#include "EStructuralChannel.generated.h"

UENUM(BlueprintType)
enum class EStructuralChannel : uint8
{
	Structure,
	Light,
	Generator,
	Extractor,
	Manufacturer,
	Transport,
	Misc,
	Switch
};

/** Save-stable key for a union-find structure component (smallest member node id). */
USTRUCT(BlueprintType)
struct STRUCTURALPOWER_API FStructuralComponentKey
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FStructuralNodeId CanonicalNodeId;

	bool IsValid() const { return CanonicalNodeId.IsValid(); }

	bool operator==(const FStructuralComponentKey& Other) const
	{
		return CanonicalNodeId == Other.CanonicalNodeId;
	}

	friend uint32 GetTypeHash(const FStructuralComponentKey& Key)
	{
		return GetTypeHash(Key.CanonicalNodeId);
	}
};

/** Resolved routing key — Tag + Effective Id used by the bus router (DR-008). */
USTRUCT(BlueprintType)
struct STRUCTURALPOWER_API FStructuralChannelKey
{
	GENERATED_BODY()

	UPROPERTY()
	EStructuralChannel Tag = EStructuralChannel::Structure;

	UPROPERTY()
	FName EffectiveId = NAME_None;

	bool operator==(const FStructuralChannelKey& Other) const
	{
		return Tag == Other.Tag && EffectiveId == Other.EffectiveId;
	}
};

STRUCTURALPOWER_API const TCHAR* StructuralChannelToString(EStructuralChannel Tag);
