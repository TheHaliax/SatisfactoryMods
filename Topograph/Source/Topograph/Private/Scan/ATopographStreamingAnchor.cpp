// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Scan/ATopographStreamingAnchor.h"

#include "Components/SceneComponent.h"
#include "WorldPartition/WorldPartitionStreamingSource.h"

ATopographStreamingAnchor::ATopographStreamingAnchor() {
  PrimaryActorTick.bCanEverTick = false;
  bReplicates = false;
  SetCanBeDamaged(false);

  RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
  StreamingSource = CreateDefaultSubobject<UWorldPartitionStreamingSourceComponent>(
      TEXT("StreamingSource"));
  if (StreamingSource) {
    StreamingSource->Priority = EStreamingSourcePriority::Highest;
  }
}

void ATopographStreamingAnchor::ConfigureSource(const FVector& WorldLocation,
                                                float RadiusUU) {
  SetActorLocation(WorldLocation);
  if (!StreamingSource) {
    return;
  }
  FStreamingSourceShape Shape;
  Shape.bUseGridLoadingRange = false;
  Shape.Radius = FMath::Max(1000.f, RadiusUU);
  Shape.Location = FVector::ZeroVector;
  StreamingSource->Shapes.Reset();
  StreamingSource->Shapes.Add(Shape);
  StreamingSource->EnableStreamingSource();
}
