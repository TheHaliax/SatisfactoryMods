// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Core/FStructuralNodeId.h"
#include "FStructuralNodeRecord.generated.h"

USTRUCT(BlueprintType)
struct STRUCTURALPOWER_API FStructuralNodeRecord
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FStructuralNodeId NodeId;

	UPROPERTY(SaveGame)
	FVector WorldLocation = FVector::ZeroVector;

	UPROPERTY(SaveGame)
	TArray<FStructuralNodeId> NeighborIds;

	UPROPERTY(SaveGame)
	bool bIsOutlet = false;
};
