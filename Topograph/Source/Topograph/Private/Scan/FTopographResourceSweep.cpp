// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Scan/FTopographResourceSweep.h"

#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "Resources/FGResourceDescriptor.h"
#include "Resources/FGResourceNode.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "TopographLog.h"

void FTopographResourceSweep::Reset() {
  Records.Reset();
  PixelToRegistry.Reset();
}

void FTopographResourceSweep::SweepWorldAccumulate(UWorld* World,
                                                   float FoundationCellUU,
                                                   float RoiMinX,
                                                   float RoiMinY) {
  if (!World || FoundationCellUU <= KINDA_SMALL_NUMBER) {
    return;
  }

  for (TActorIterator<AFGResourceNode> It(World); It; ++It) {
    AFGResourceNode* Node = *It;
    if (!IsValid(Node)) {
      continue;
    }

    const FString Name = Node->GetName();
    bool bDup = false;
    for (const FTopographResourceRecord& Existing : Records) {
      if (Existing.ActorName == Name &&
          Existing.Location.Equals(Node->GetActorLocation(), 1.f)) {
        bDup = true;
        break;
      }
    }
    if (bDup) {
      continue;
    }

    FTopographResourceRecord Rec;
    Rec.RegistryIndex = Records.Num();
    Rec.ActorName = Name;
    Rec.Location = Node->GetActorLocation();
    Rec.Rotation = Node->GetActorRotation();
    Rec.Purity = static_cast<int32>(Node->GetResourcePurity());
    Rec.FoundationIx = FMath::FloorToInt(
        (Rec.Location.X - RoiMinX) / FoundationCellUU);
    Rec.FoundationIy = FMath::FloorToInt(
        (Rec.Location.Y - RoiMinY) / FoundationCellUU);

    if (TSubclassOf<UFGResourceDescriptor> Cls = Node->GetResourceClass()) {
      Rec.ResourceClassName = Cls->GetName();
    } else {
      Rec.ResourceClassName = TEXT("Unknown");
    }

    Records.Add(MoveTemp(Rec));
  }

  UE_LOG(LogTopograph, Log, TEXT("Resource sweep accumulate: %d nodes"),
         Records.Num());
}

void FTopographResourceSweep::SweepWorld(UWorld* World, float FoundationCellUU,
                                         float RoiMinX, float RoiMinY) {
  Reset();
  SweepWorldAccumulate(World, FoundationCellUU, RoiMinX, RoiMinY);
}

uint32 FTopographResourceSweep::LookupResourceParent(float WorldX, float WorldY,
                                                     float SampleStepUU) const {
  if (Records.Num() == 0) {
    return 0;
  }
  // Match node whose XY is within half a sample of this column.
  const float Half = SampleStepUU * 0.5f;
  for (const FTopographResourceRecord& Rec : Records) {
    if (FMath::Abs(Rec.Location.X - WorldX) <= Half &&
        FMath::Abs(Rec.Location.Y - WorldY) <= Half) {
      return static_cast<uint32>(Rec.RegistryIndex + 1);
    }
  }
  return 0;
}

bool FTopographResourceSweep::WriteJson(const FString& AbsolutePath) const {
  TArray<TSharedPtr<FJsonValue>> Arr;
  for (const FTopographResourceRecord& Rec : Records) {
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetNumberField(TEXT("index"), Rec.RegistryIndex);
    Obj->SetStringField(TEXT("actor"), Rec.ActorName);
    Obj->SetStringField(TEXT("resource"), Rec.ResourceClassName);
    Obj->SetNumberField(TEXT("purity"), Rec.Purity);
    Obj->SetNumberField(TEXT("x"), Rec.Location.X);
    Obj->SetNumberField(TEXT("y"), Rec.Location.Y);
    Obj->SetNumberField(TEXT("z"), Rec.Location.Z);
    Obj->SetNumberField(TEXT("yaw"), Rec.Rotation.Yaw);
    Obj->SetNumberField(TEXT("foundation_ix"), Rec.FoundationIx);
    Obj->SetNumberField(TEXT("foundation_iy"), Rec.FoundationIy);
    Arr.Add(MakeShared<FJsonValueObject>(Obj));
  }

  FString Out;
  TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
  FJsonSerializer::Serialize(Arr, Writer);
  return FFileHelper::SaveStringToFile(Out, *AbsolutePath);
}
