// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/WorldPartitionStreamingSourceComponent.h"
#include "ATopographStreamingAnchor.generated.h"

/**
 * Server-side WP streaming source for one macro-tile.
 * No pawn / viewport required.
 */
UCLASS(NotPlaceable, Transient)
class TOPOGRAPH_API ATopographStreamingAnchor : public AActor {
  GENERATED_BODY()

 public:
  ATopographStreamingAnchor();

  void ConfigureSource(const FVector& WorldLocation, float RadiusUU);

  UPROPERTY(VisibleAnywhere, Category = "Topograph")
  TObjectPtr<UWorldPartitionStreamingSourceComponent> StreamingSource = nullptr;
};
