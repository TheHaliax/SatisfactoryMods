// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Lightweight/FStructuralLightweightIndex.h"

#include "Buildables/FGBuildable.h"
#include "FGLightweightBuildableSubsystem.h"
#include "Graph/FStructuralAdjacencyHeuristics.h"
#include "Graph/FStructuralOutletParentHeuristics.h"
#include "Rules/FStructuralEligibilityRules.h"
#include "StructuralPowerConstants.h"

namespace {
static AFGLightweightBuildableSubsystem* GetLightweightSubsystem(UWorld* World) {
  return IsValid(World) ? AFGLightweightBuildableSubsystem::Get(World) : nullptr;
}
}

FIntVector FStructuralLightweightIndex::ToCell(const FVector& Location) {
  return FIntVector(FMath::FloorToInt(Location.X / StructuralPowerConstants::GridCellCm),
                    FMath::FloorToInt(Location.Y / StructuralPowerConstants::GridCellCm),
                    FMath::FloorToInt(Location.Z / StructuralPowerConstants::GridCellCm));
}

FBox FStructuralLightweightIndex::GetWorldBounds(TSubclassOf<AFGBuildable> BuildableClass,
                                                 const FTransform& Transform,
                                                 const FBox& LocalBounds) {
  if (LocalBounds.IsValid) {
    return LocalBounds.TransformBy(Transform);
  }

  const float Extent = StructuralPowerConstants::DefaultFoundationExtentCm;
  const FVector HalfExtents(Extent, Extent, Extent);
  return FBox(Transform.GetLocation() - HalfExtents, Transform.GetLocation() + HalfExtents);
}

bool FStructuralLightweightIndex::AreBoundsStructurallyConnected(const FBox& A, const FBox& B,
                                                                 TSubclassOf<AFGBuildable> ClassA,
                                                                 TSubclassOf<AFGBuildable> ClassB) {
  if (!A.IsValid || !B.IsValid) {
    return false;
  }

  const float Gap = FStructuralAdjacencyHeuristics::GetStructuralGapCm(ClassA, ClassB);

  auto ExpandForClass = [Gap](const FBox& Box, TSubclassOf<AFGBuildable> BuildableClass) {
    FBox Expanded = Box.ExpandBy(Gap);
    if (!BuildableClass) {
      return Expanded;
    }

    const AFGBuildable* CDO = BuildableClass->GetDefaultObject<AFGBuildable>();
    if (FStructuralOutletParentHeuristics::IsPreferredWallClass(CDO)) {
      Expanded.Min.Z -= 100.0f;
    } else if (FStructuralOutletParentHeuristics::IsPreferredFoundationClass(CDO)) {
      Expanded.Max.Z += 100.0f;
    } else if (FStructuralAdjacencyHeuristics::IsBeamClass(BuildableClass)) {
      Expanded = Expanded.ExpandBy(StructuralPowerConstants::BeamOverlapPaddingCm);
    }

    return Expanded;
  };

  if (ExpandForClass(A, ClassA).Intersect(ExpandForClass(B, ClassB))) {
    return true;
  }

  return FStructuralAdjacencyHeuristics::AreAdjacencyBoundsConnected(A, B, ClassA, ClassB);
}

void FStructuralLightweightIndex::IndexMemberCells(int32 MemberIndex, const FBox& WorldBounds) {
  if (!WorldBounds.IsValid) {
    return;
  }

  const FIntVector MinCell = ToCell(WorldBounds.Min);
  const FIntVector MaxCell = ToCell(WorldBounds.Max);
  for (int32 X = MinCell.X; X <= MaxCell.X; ++X) {
    for (int32 Y = MinCell.Y; Y <= MaxCell.Y; ++Y) {
      for (int32 Z = MinCell.Z; Z <= MaxCell.Z; ++Z) {
        CellToIndices.FindOrAdd(FIntVector(X, Y, Z)).AddUnique(MemberIndex);
      }
    }
  }
}

void FStructuralLightweightIndex::UnindexMemberCells(int32 MemberIndex, const FBox& WorldBounds) {
  if (!WorldBounds.IsValid) {
    return;
  }

  const FIntVector MinCell = ToCell(WorldBounds.Min);
  const FIntVector MaxCell = ToCell(WorldBounds.Max);
  for (int32 X = MinCell.X; X <= MaxCell.X; ++X) {
    for (int32 Y = MinCell.Y; Y <= MaxCell.Y; ++Y) {
      for (int32 Z = MinCell.Z; Z <= MaxCell.Z; ++Z) {
        if (TArray<int32>* Indices = CellToIndices.Find(FIntVector(X, Y, Z))) {
          Indices->Remove(MemberIndex);
          if (Indices->Num() == 0) {
            CellToIndices.Remove(FIntVector(X, Y, Z));
          }
        }
      }
    }
  }
}

bool FStructuralLightweightIndex::RegisterTrackedMember(UWorld* World,
                                                        const FStructuralLightweightKey& Key) {
  if (!Key.IsValid() || KeyToIndex.Contains(Key)) {
    return KeyToIndex.Contains(Key);
  }

  AFGLightweightBuildableSubsystem* LightweightSubsystem = GetLightweightSubsystem(World);
  if (!IsValid(LightweightSubsystem)) {
    return false;
  }

  const TArray<FRuntimeBuildableInstanceData>* Instances =
      LightweightSubsystem->GetAllLightweightBuildableInstances().Find(Key.BuildableClass);
  if (!Instances || !Instances->IsValidIndex(Key.Index)) {
    return false;
  }

  const FRuntimeBuildableInstanceData& RuntimeData = (*Instances)[Key.Index];
  if (!RuntimeData.IsValidOnLoad()) {
    return false;
  }

  const AFGBuildable* CDO = Key.BuildableClass->GetDefaultObject<AFGBuildable>();
  if (!FStructuralEligibilityRules::IsBusMember(CDO)) {
    return false;
  }

  FIndexedMember Member;
  Member.Key = Key;
  Member.WorldBounds =
      GetWorldBounds(Key.BuildableClass, RuntimeData.Transform, RuntimeData.BoundingBox);
  if (!Member.WorldBounds.IsValid) {
    return false;
  }

  const int32 MemberIndex = Members.Add(Member);
  KeyToIndex.Add(Key, MemberIndex);
  IndexMemberCells(MemberIndex, Member.WorldBounds);
  return true;
}

void FStructuralLightweightIndex::UnregisterMember(const FStructuralLightweightKey& Key) {
  const int32* MemberIndexPtr = KeyToIndex.Find(Key);
  if (!MemberIndexPtr || !Members.IsValidIndex(*MemberIndexPtr)) {
    KeyToIndex.Remove(Key);
    return;
  }

  const int32 RemoveIndex = *MemberIndexPtr;
  UnindexMemberCells(RemoveIndex, Members[RemoveIndex].WorldBounds);
  KeyToIndex.Remove(Key);

  const int32 LastIndex = Members.Num() - 1;
  if (RemoveIndex != LastIndex) {
    UnindexMemberCells(LastIndex, Members[LastIndex].WorldBounds);
    Members.Swap(RemoveIndex, LastIndex);
    Members.SetNum(LastIndex);
    KeyToIndex.Add(Members[RemoveIndex].Key, RemoveIndex);
    IndexMemberCells(RemoveIndex, Members[RemoveIndex].WorldBounds);
  } else {
    Members.SetNum(LastIndex);
  }
}

FStructuralWallAnchor
FStructuralLightweightIndex::FindParentWallForOutlet(AFGBuildable* Outlet) const {
  if (!IsValid(Outlet) || Members.Num() == 0) {
    return {};
  }

  const FVector AnchorLocation = FStructuralOutletParentHeuristics::GetOutletAnchorLocation(Outlet);
  const FIntVector Origin = ToCell(AnchorLocation);
  const int32 CellRadius = FMath::CeilToInt(StructuralPowerConstants::MaxOutletParentDistCm /
                                            StructuralPowerConstants::GridCellCm);

  const bool bPreferFoundation = FStructuralOutletParentHeuristics::PrefersFoundationAnchor(Outlet);

  FStructuralWallAnchor BestPreferred;
  float BestPreferredScore = TNumericLimits<float>::Max();
  FStructuralWallAnchor BestAny;
  float BestAnyScore = TNumericLimits<float>::Max();

  for (int32 DX = -CellRadius; DX <= CellRadius; ++DX) {
    for (int32 DY = -CellRadius; DY <= CellRadius; ++DY) {
      for (int32 DZ = -CellRadius; DZ <= CellRadius; ++DZ) {
        const TArray<int32>* Indices = CellToIndices.Find(Origin + FIntVector(DX, DY, DZ));
        if (!Indices) {
          continue;
        }

        for (int32 MemberIndex : *Indices) {
          const FIndexedMember& Member = Members[MemberIndex];
          const AFGBuildable* CDO = Member.Key.BuildableClass->GetDefaultObject<AFGBuildable>();

          const bool bNear = FStructuralOutletParentHeuristics::IsOutletNearBounds(
              Member.WorldBounds, Member.Key.BuildableClass, Outlet);
          if (!bNear) {
            continue;
          }

          FStructuralWallAnchor Candidate;
          Candidate.Lightweight = Member.Key;
          Candidate.WorldLocation = Member.WorldBounds.GetClosestPointTo(AnchorLocation);
          const float Score = FStructuralOutletParentHeuristics::ScoreParentCandidate(
              AnchorLocation, Member.WorldBounds, Member.Key.BuildableClass, Outlet);
          const bool bPreferredClass =
              bPreferFoundation ? FStructuralOutletParentHeuristics::IsPreferredFoundationClass(CDO)
                                : FStructuralOutletParentHeuristics::IsPreferredWallClass(CDO);
          if (bPreferredClass && Score < BestPreferredScore) {
            BestPreferredScore = Score;
            BestPreferred = Candidate;
          } else if (Score < BestAnyScore) {
            BestAnyScore = Score;
            BestAny = Candidate;
          }
        }
      }
    }
  }

  return BestPreferred.IsValid() ? BestPreferred : BestAny;
}

FStructuralNodeId FStructuralLightweightIndex::MakeNodeId(const FStructuralLightweightKey& Key) {
  FStructuralNodeId Id;
  Id.BuildableClass = FSoftClassPath(Key.BuildableClass);
  Id.LightweightIndex = Key.Index;
  Id.ActorName = NAME_None;
  return Id;
}
