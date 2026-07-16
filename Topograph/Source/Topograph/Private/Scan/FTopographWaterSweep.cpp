// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Scan/FTopographWaterSweep.h"

#include "EngineUtils.h"
#include "FGWaterVolume.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "TopographLog.h"

void FTopographWaterSweep::Reset() { Records.Reset(); }

void FTopographWaterSweep::SweepWorldAccumulate(UWorld* World) {
  if (!World) {
    return;
  }

  for (TActorIterator<AFGWaterVolume> It(World); It; ++It) {
    AFGWaterVolume* Vol = *It;
    if (!IsValid(Vol)) {
      continue;
    }

    const FString Name = Vol->GetName();
    bool bDup = false;
    for (const FRecord& Existing : Records) {
      if (Existing.ActorName == Name) {
        bDup = true;
        break;
      }
    }
    if (bDup) {
      continue;
    }

    FRecord Rec;
    Rec.ActorName = Name;
    Rec.Bounds = Vol->GetComponentsBoundingBox(true);
    Rec.Volume = Vol;
    if (!Rec.Bounds.IsValid) {
      continue;
    }
    Records.Add(MoveTemp(Rec));
  }

  UE_LOG(LogTopograph, Log, TEXT("Water sweep accumulate: %d volumes"),
         Records.Num());
}

bool FTopographWaterSweep::TryGetMinFloorZ(float& OutFloorZ) const {
  if (Records.Num() == 0) {
    return false;
  }
  float MinZ = TNumericLimits<float>::Max();
  for (const FRecord& Rec : Records) {
    MinZ = FMath::Min(MinZ, Rec.Bounds.Min.Z);
  }
  OutFloorZ = MinZ;
  return true;
}

FTopographWaterColumn FTopographWaterSweep::LookupColumn(float WorldX,
                                                         float WorldY) const {
  FTopographWaterColumn Out;
  for (const FRecord& Rec : Records) {
    if (WorldX < Rec.Bounds.Min.X || WorldX > Rec.Bounds.Max.X ||
        WorldY < Rec.Bounds.Min.Y || WorldY > Rec.Bounds.Max.Y) {
      continue;
    }

    AFGWaterVolume* Vol = Rec.Volume.Get();
    if (!IsValid(Vol)) {
      continue;
    }

    // Sample near free surface — brush may not fill full AABB height.
    const float TestZ =
        FMath::Lerp(Rec.Bounds.Min.Z, Rec.Bounds.Max.Z, 0.85f);
    const FVector Test(WorldX, WorldY, TestZ);
    float Dist = 0.f;
    if (!Vol->EncompassesPoint(Test, 0.f, &Dist)) {
      // Retry mid-column for deep / odd brushes.
      const FVector Mid(WorldX, WorldY, Rec.Bounds.GetCenter().Z);
      if (!Vol->EncompassesPoint(Mid, 0.f, &Dist)) {
        continue;
      }
    }

    if (!Out.bInWater) {
      Out.bInWater = true;
      Out.SurfaceZ = Rec.Bounds.Max.Z;
      Out.FloorZ = Rec.Bounds.Min.Z;
    } else {
      Out.SurfaceZ = FMath::Max(Out.SurfaceZ, Rec.Bounds.Max.Z);
      Out.FloorZ = FMath::Min(Out.FloorZ, Rec.Bounds.Min.Z);
    }
  }
  return Out;
}

bool FTopographWaterSweep::WriteJson(const FString& AbsolutePath) const {
  TArray<TSharedPtr<FJsonValue>> Arr;
  for (int32 i = 0; i < Records.Num(); ++i) {
    const FRecord& Rec = Records[i];
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetNumberField(TEXT("index"), i);
    Obj->SetStringField(TEXT("actor"), Rec.ActorName);
    Obj->SetNumberField(TEXT("min_x"), Rec.Bounds.Min.X);
    Obj->SetNumberField(TEXT("min_y"), Rec.Bounds.Min.Y);
    Obj->SetNumberField(TEXT("min_z"), Rec.Bounds.Min.Z);
    Obj->SetNumberField(TEXT("max_x"), Rec.Bounds.Max.X);
    Obj->SetNumberField(TEXT("max_y"), Rec.Bounds.Max.Y);
    Obj->SetNumberField(TEXT("max_z"), Rec.Bounds.Max.Z);
    Arr.Add(MakeShared<FJsonValueObject>(Obj));
  }

  FString Out;
  TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
  FJsonSerializer::Serialize(Arr, Writer);
  return FFileHelper::SaveStringToFile(Out, *AbsolutePath);
}
