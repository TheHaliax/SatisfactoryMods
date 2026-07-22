// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Appearance/FPCMetallicFinishPool.h"

#include "Appearance/FPCMetallicColorCorrection.h"
#include "PipelineColorLog.h"
#include "PipelineColorRootInstanceModule.h"
#include "Reflection/ClassGenerator.h"
#include "Swatches/UPCFinishDescs.h"

namespace {
void BakeFinishCdo(UClass* Generated, float Roughness, float Metallic, const FText& Name) {
  if (!Generated) {
    return;
  }
  UFGFactoryCustomizationDescriptor_PaintFinish* CDO =
      Cast<UFGFactoryCustomizationDescriptor_PaintFinish>(Generated->GetDefaultObject());
  if (!CDO) {
    return;
  }
  CDO->RoughnessValue = Roughness;
  CDO->MetallicValue = Metallic;
  CDO->mHasForcedColor = false;
  CDO->mForcedColor = FLinearColor::White;
  CDO->ID = INDEX_CUSTOM_COLOR_SLOT;
  CDO->mUseDisplayNameAndDescription = true;
  CDO->mDisplayName = Name;
  CDO->mValidBuildables.Reset();

  UPCFinish_MetallicColor::EnsureIconLoaded();
  if (const UPCFinish_MetallicColor* Template = GetDefault<UPCFinish_MetallicColor>()) {
    CDO->mIcon = Template->mIcon;
  }
}

float BucketRoughness(int32 Index) {
  const float Lo = FPCMetallicColorCorrection::AlphaMin;
  const float Hi = FPCMetallicColorCorrection::AlphaMax;
  if (FPCMetallicFinishPool::BucketCount <= 1) {
    return Lo;
  }
  const float T =
      static_cast<float>(Index) / static_cast<float>(FPCMetallicFinishPool::BucketCount - 1);
  return FMath::Lerp(Lo, Hi, T);
}

int32 BucketIndex(float Roughness) {
  const float Lo = FPCMetallicColorCorrection::AlphaMin;
  const float Hi = FPCMetallicColorCorrection::AlphaMax;
  const float Span = FMath::Max(Hi - Lo, KINDA_SMALL_NUMBER);
  const float T = FMath::Clamp((Roughness - Lo) / Span, 0.f, 1.f);
  return FMath::Clamp(FMath::RoundToInt(T * (FPCMetallicFinishPool::BucketCount - 1)), 0,
                      FPCMetallicFinishPool::BucketCount - 1);
}
} // namespace

void FPCMetallicFinishPool::EnsureCreated(UPipelineColorRootInstanceModule* Root) {
  if (!IsValid(Root) || Root->bMetallicFinishPoolReady) {
    return;
  }

  Root->MetallicFinishBuckets.Reset();
  Root->MetallicFinishBuckets.Reserve(BucketCount);

  for (int32 i = 0; i < BucketCount; ++i) {
    const FString ClassName = FString::Printf(TEXT("PCFinish_M%02d"), i);
    UClass* Generated = FClassGenerator::GenerateSimpleClass(
        TEXT("/PipelineColor"), *ClassName, UPCFinish_MetallicColor::StaticClass());
    if (!Generated) {
      UE_LOG(LogPipelineColor, Error, TEXT("%s ClassGen metallic bucket %d failed"),
             PIPELINECOLOR_LOG_PREFIX, i);
      continue;
    }
    const float R = BucketRoughness(i);
    BakeFinishCdo(Generated, R, 1.f,
                  FText::FromString(FString::Printf(TEXT("PC Metallic %.3f"), R)));
    Root->MetallicFinishBuckets.Add(Generated);
  }

  {
    UClass* Generated = FClassGenerator::GenerateSimpleClass(
        TEXT("/PipelineColor"), TEXT("PCFinish_MetallicNeutral"),
        UPCFinish_MetallicColor::StaticClass());
    if (Generated) {
      BakeFinishCdo(Generated, FPCMetallicColorCorrection::NeutralMetallicRoughness, 1.f,
                    FText::FromString(TEXT("PC Metallic Neutral")));
      Root->MetallicNeutralFinish = Generated;
    } else {
      UE_LOG(LogPipelineColor, Error, TEXT("%s ClassGen MetallicNeutral failed"),
             PIPELINECOLOR_LOG_PREFIX);
      Root->MetallicNeutralFinish = UPCFinish_MetallicColor::StaticClass();
    }
  }

  Root->bMetallicFinishPoolReady =
      Root->MetallicFinishBuckets.Num() == BucketCount && Root->MetallicNeutralFinish != nullptr;
  UE_LOG(LogPipelineColor, Log, TEXT("%s metallic finish pool buckets=%d ready=%d"),
         PIPELINECOLOR_LOG_PREFIX, Root->MetallicFinishBuckets.Num(),
         Root->bMetallicFinishPoolReady ? 1 : 0);
}

TSubclassOf<UFGFactoryCustomizationDescriptor_PaintFinish>
FPCMetallicFinishPool::Resolve(UPipelineColorRootInstanceModule* Root, float Roughness) {
  EnsureCreated(Root);
  if (!IsValid(Root)) {
    return UPCFinish_MetallicColor::StaticClass();
  }

  if (FMath::IsNearlyEqual(Roughness, FPCMetallicColorCorrection::NeutralMetallicRoughness,
                           0.05f) ||
      Roughness > FPCMetallicColorCorrection::AlphaMax + 0.05f) {
    if (Root->MetallicNeutralFinish) {
      return Root->MetallicNeutralFinish;
    }
    return UPCFinish_MetallicColor::StaticClass();
  }

  const int32 Index = BucketIndex(Roughness);
  if (Root->MetallicFinishBuckets.IsValidIndex(Index) && Root->MetallicFinishBuckets[Index]) {
    return Root->MetallicFinishBuckets[Index];
  }
  return UPCFinish_MetallicColor::StaticClass();
}

TSubclassOf<UFGFactoryCustomizationDescriptor_PaintFinish>
FPCMetallicFinishPool::GetMatteNeutral() {
  return UPCFinish_MatteNeutral::StaticClass();
}
