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

/** Per-buildable player overrides — NAME_None field = inherit role default (DR-011). */
USTRUCT(BlueprintType)
struct STRUCTURALPOWER_API FStructuralEndpointOverrides
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FName SourceOverride = NAME_None;

	UPROPERTY(SaveGame)
	FName ControlOverride = NAME_None;

	bool HasAnyOverride() const
	{
		return !SourceOverride.IsNone() || !ControlOverride.IsNone();
	}
};

/** Island-scoped id pool for config dropdowns (DR-014). */
USTRUCT(BlueprintType)
struct STRUCTURALPOWER_API FStructuralComponentIdList
{
	GENERATED_BODY()

	UPROPERTY()
	FName DefaultSourceId = NAME_None;

	UPROPERTY()
	TArray<FName> NamedSourceIds;

	UPROPERTY()
	TArray<FName> NamedControlIds;

	/** Switch gate ids (DR-014) — eligible as Source on any endpoint. */
	UPROPERTY()
	TArray<FName> NamedSwitchControlIds;

	/** Panel light-group ids (DR-014) — Source dropdown on lights only. */
	UPROPERTY()
	TArray<FName> NamedLightGroupIds;

	UPROPERTY()
	FName ResolvedSource = NAME_None;

	UPROPERTY()
	FName ResolvedControl = NAME_None;

	UPROPERTY()
	FName SourceOverride = NAME_None;

	UPROPERTY()
	FName ControlOverride = NAME_None;
};

/** Resolved routing key — Tag + Source/Control (DR-011) or Effective Id (DR-008 legacy). */
USTRUCT(BlueprintType)
struct STRUCTURALPOWER_API FStructuralChannelKey
{
	GENERATED_BODY()

	UPROPERTY()
	EStructuralChannel Tag = EStructuralChannel::Structure;

	/** Legacy single namespace — generators/factories until v2.3+. */
	UPROPERTY()
	FName EffectiveId = NAME_None;

	UPROPERTY()
	FName Source = NAME_None;

	UPROPERTY()
	FName Control = NAME_None;

	bool operator==(const FStructuralChannelKey& Other) const
	{
		return Tag == Other.Tag
			&& EffectiveId == Other.EffectiveId
			&& Source == Other.Source
			&& Control == Other.Control;
	}
};

STRUCTURALPOWER_API const TCHAR* StructuralChannelToString(EStructuralChannel Tag);
