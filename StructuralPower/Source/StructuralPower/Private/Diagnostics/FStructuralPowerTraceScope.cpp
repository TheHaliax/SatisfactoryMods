// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Diagnostics/FStructuralPowerTraceScope.h"

#include "Config/FStructuralPowerModConfig.h"
#include "Diagnostics/FStructuralPowerTraceUtil.h"
#include "HAL/PlatformTime.h"
#include "StructuralPowerLog.h"

namespace {
thread_local int32 GTraceScopeDepth = 0;
}  // namespace

FStructuralPowerTraceScope::FStructuralPowerTraceScope(const TCHAR* InDomain, const TCHAR* InOp,
                                                       const FString& InDetail)
    : Domain(InDomain), Op(InOp), Detail(InDetail) {
  if (!FStructuralPowerModConfig::IsTraceEnabled() || !Domain || !Op) {
    return;
  }

  bActive = true;
  Depth = GTraceScopeDepth++;
  StartCycles = FPlatformTime::Cycles64();

  if (Detail.IsEmpty()) {
    UE_LOG(LogStructuralPower, Log, TEXT("[HALSP] trace> %s.%s frame=%d depth=%d"), Domain, Op,
           FStructuralPowerTraceUtil::GetFrameNumber(), Depth);
  } else {
    UE_LOG(LogStructuralPower, Log, TEXT("[HALSP] trace> %s.%s frame=%d depth=%d %s"), Domain, Op,
           FStructuralPowerTraceUtil::GetFrameNumber(), Depth, *Detail);
  }
}

FStructuralPowerTraceScope::~FStructuralPowerTraceScope() {
  if (!bActive) {
    return;
  }

  const double ElapsedMs = FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - StartCycles);

  if (Detail.IsEmpty()) {
    UE_LOG(LogStructuralPower, Log, TEXT("[HALSP] trace< %s.%s ms=%.3f frame=%d depth=%d"), Domain,
           Op, ElapsedMs, FStructuralPowerTraceUtil::GetFrameNumber(), Depth);
  } else {
    UE_LOG(LogStructuralPower, Log, TEXT("[HALSP] trace< %s.%s ms=%.3f frame=%d depth=%d %s"),
           Domain, Op, ElapsedMs, FStructuralPowerTraceUtil::GetFrameNumber(), Depth, *Detail);
  }

  GTraceScopeDepth = FMath::Max(0, GTraceScopeDepth - 1);
}
