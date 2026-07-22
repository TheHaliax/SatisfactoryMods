// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "FStructuralGraphSaveRecords.generated.h"

// Flat records — SCIM save parser cannot walk SoftClassPath members or struct-keyed TMaps.
USTRUCT()
struct STRUCTURALPOWER_API FStructuralNodeIdSaveRecord {
  GENERATED_BODY()

  UPROPERTY(SaveGame)
  FString BuildableClassPath;

  UPROPERTY(SaveGame)
  FName ActorName = NAME_None;

  UPROPERTY(SaveGame)
  int32 LightweightIndex = INDEX_NONE;
};

USTRUCT()
struct STRUCTURALPOWER_API FStructuralDefaultIdSaveRecord {
  GENERATED_BODY()

  UPROPERTY(SaveGame)
  FStructuralNodeIdSaveRecord Node;

  UPROPERTY(SaveGame)
  FName DefaultId = NAME_None;
};

USTRUCT()
struct STRUCTURALPOWER_API FStructuralEndpointOverrideSaveRecord {
  GENERATED_BODY()

  UPROPERTY(SaveGame)
  FStructuralNodeIdSaveRecord Node;

  UPROPERTY(SaveGame)
  FName SourceOverride = NAME_None;

  UPROPERTY(SaveGame)
  FName ControlOverride = NAME_None;

  UPROPERTY(SaveGame)
  bool bGlobalControl = false;
};
