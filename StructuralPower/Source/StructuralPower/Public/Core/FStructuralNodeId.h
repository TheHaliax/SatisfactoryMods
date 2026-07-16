// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "FStructuralNodeId.generated.h"

USTRUCT(BlueprintType)
struct STRUCTURALPOWER_API FStructuralNodeId {
  GENERATED_BODY()

  UPROPERTY(SaveGame)
  FSoftClassPath BuildableClass;

  UPROPERTY(SaveGame)
  FName ActorName;

  UPROPERTY(SaveGame)
  int32 LightweightIndex = INDEX_NONE;

  bool IsValid() const {
    return !BuildableClass.IsNull() && (!ActorName.IsNone() || LightweightIndex != INDEX_NONE);
  }

  bool IsLightweight() const {
    return LightweightIndex != INDEX_NONE && ActorName.IsNone();
  }

  bool operator==(const FStructuralNodeId& Other) const {
    return BuildableClass == Other.BuildableClass && ActorName == Other.ActorName &&
           LightweightIndex == Other.LightweightIndex;
  }

  friend uint32 GetTypeHash(const FStructuralNodeId& Id) {
    return HashCombine(HashCombine(GetTypeHash(Id.BuildableClass), GetTypeHash(Id.ActorName)),
                       ::GetTypeHash(Id.LightweightIndex));
  }
};
