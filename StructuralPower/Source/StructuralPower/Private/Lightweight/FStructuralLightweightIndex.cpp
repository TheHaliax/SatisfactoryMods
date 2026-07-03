// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Lightweight/FStructuralLightweightIndex.h"

#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableCornerWall.h"
#include "Buildables/FGBuildableFoundation.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "Buildables/FGBuildableRamp.h"
#include "Buildables/FGBuildableWall.h"
#include "Components/UFGStructuralPowerConnectionComponent.h"
#include "FGLightweightBuildableSubsystem.h"
#include "Graph/FStructuralOutletParentHeuristics.h"
#include "Graph/FStructuralAdjacencyHeuristics.h"
#include "Rules/FStructuralEligibilityRules.h"
#include "StructuralPowerConstants.h"
#include "StructuralPowerLog.h"

namespace
{
static AFGLightweightBuildableSubsystem* GetLightweightSubsystem(UWorld* World)
{
	return IsValid(World) ? AFGLightweightBuildableSubsystem::Get(World) : nullptr;
}
}

FIntVector FStructuralLightweightIndex::ToCell(const FVector& Location)
{
	return FIntVector(
		FMath::FloorToInt(Location.X / StructuralPowerConstants::GridCellCm),
		FMath::FloorToInt(Location.Y / StructuralPowerConstants::GridCellCm),
		FMath::FloorToInt(Location.Z / StructuralPowerConstants::GridCellCm));
}

FBox FStructuralLightweightIndex::GetWorldBounds(
	TSubclassOf<AFGBuildable> BuildableClass,
	const FTransform& Transform,
	const FBox& LocalBounds)
{
	if (LocalBounds.IsValid)
	{
		return LocalBounds.TransformBy(Transform);
	}

	const float Extent = StructuralPowerConstants::DefaultFoundationExtentCm;
	const FVector HalfExtents(Extent, Extent, Extent);
	return FBox(Transform.GetLocation() - HalfExtents, Transform.GetLocation() + HalfExtents);
}

bool FStructuralLightweightIndex::AreBoundsStructurallyConnected(
	const FBox& A,
	const FBox& B,
	TSubclassOf<AFGBuildable> ClassA,
	TSubclassOf<AFGBuildable> ClassB)
{
	if (!A.IsValid || !B.IsValid)
	{
		return false;
	}

	const float Gap = FStructuralAdjacencyHeuristics::GetStructuralGapCm(ClassA, ClassB);

	auto ExpandForClass = [Gap](const FBox& Box, TSubclassOf<AFGBuildable> BuildableClass)
	{
		FBox Expanded = Box.ExpandBy(Gap);
		if (!BuildableClass)
		{
			return Expanded;
		}

		const AFGBuildable* CDO = BuildableClass->GetDefaultObject<AFGBuildable>();
		if (FStructuralOutletParentHeuristics::IsPreferredWallClass(CDO))
		{
			Expanded.Min.Z -= 100.0f;
		}
		else if (FStructuralOutletParentHeuristics::IsPreferredFoundationClass(CDO))
		{
			Expanded.Max.Z += 100.0f;
		}
		else if (FStructuralAdjacencyHeuristics::IsBeamClass(BuildableClass))
		{
			Expanded = Expanded.ExpandBy(StructuralPowerConstants::BeamOverlapPaddingCm);
		}

		return Expanded;
	};

	if (ExpandForClass(A, ClassA).Intersect(ExpandForClass(B, ClassB)))
	{
		return true;
	}

	return FStructuralAdjacencyHeuristics::AreAdjacencyBoundsConnected(A, B, ClassA, ClassB);
}

void FStructuralLightweightIndex::IndexMemberCells(int32 MemberIndex, const FBox& WorldBounds)
{
	if (!WorldBounds.IsValid)
	{
		return;
	}

	const FIntVector MinCell = ToCell(WorldBounds.Min);
	const FIntVector MaxCell = ToCell(WorldBounds.Max);
	for (int32 X = MinCell.X; X <= MaxCell.X; ++X)
	{
		for (int32 Y = MinCell.Y; Y <= MaxCell.Y; ++Y)
		{
			for (int32 Z = MinCell.Z; Z <= MaxCell.Z; ++Z)
			{
				CellToIndices.FindOrAdd(FIntVector(X, Y, Z)).AddUnique(MemberIndex);
			}
		}
	}
}

void FStructuralLightweightIndex::UnindexMemberCells(int32 MemberIndex, const FBox& WorldBounds)
{
	if (!WorldBounds.IsValid)
	{
		return;
	}

	const FIntVector MinCell = ToCell(WorldBounds.Min);
	const FIntVector MaxCell = ToCell(WorldBounds.Max);
	for (int32 X = MinCell.X; X <= MaxCell.X; ++X)
	{
		for (int32 Y = MinCell.Y; Y <= MaxCell.Y; ++Y)
		{
			for (int32 Z = MinCell.Z; Z <= MaxCell.Z; ++Z)
			{
				if (TArray<int32>* Indices = CellToIndices.Find(FIntVector(X, Y, Z)))
				{
					Indices->Remove(MemberIndex);
					if (Indices->Num() == 0)
					{
						CellToIndices.Remove(FIntVector(X, Y, Z));
					}
				}
			}
		}
	}
}

const FStructuralLightweightIndex::FIndexedMember* FStructuralLightweightIndex::FindMember(
	const FStructuralLightweightKey& Key) const
{
	const int32* MemberIndexPtr = KeyToIndex.Find(Key);
	if (!MemberIndexPtr || !Members.IsValidIndex(*MemberIndexPtr))
	{
		return nullptr;
	}

	return &Members[*MemberIndexPtr];
}

bool FStructuralLightweightIndex::IsTracked(const FStructuralLightweightKey& Key) const
{
	return FindMember(Key) != nullptr;
}

bool FStructuralLightweightIndex::GetMemberBounds(const FStructuralLightweightKey& Key, FBox& OutBounds) const
{
	if (const FIndexedMember* Member = FindMember(Key))
	{
		OutBounds = Member->WorldBounds;
		return OutBounds.IsValid != 0;
	}

	return false;
}

bool FStructuralLightweightIndex::RegisterTrackedMember(UWorld* World, const FStructuralLightweightKey& Key)
{
	if (!Key.IsValid() || KeyToIndex.Contains(Key))
	{
		return KeyToIndex.Contains(Key);
	}

	AFGLightweightBuildableSubsystem* LightweightSubsystem = GetLightweightSubsystem(World);
	if (!IsValid(LightweightSubsystem))
	{
		return false;
	}

	const TArray<FRuntimeBuildableInstanceData>* Instances =
		LightweightSubsystem->GetAllLightweightBuildableInstances().Find(Key.BuildableClass);
	if (!Instances || !Instances->IsValidIndex(Key.Index))
	{
		return false;
	}

	const FRuntimeBuildableInstanceData& RuntimeData = (*Instances)[Key.Index];
	if (!RuntimeData.IsValidOnLoad())
	{
		return false;
	}

	const AFGBuildable* CDO = Key.BuildableClass->GetDefaultObject<AFGBuildable>();
	if (!FStructuralEligibilityRules::IsBusMember(CDO))
	{
		return false;
	}

	FIndexedMember Member;
	Member.Key = Key;
	Member.WorldBounds = GetWorldBounds(Key.BuildableClass, RuntimeData.Transform, RuntimeData.BoundingBox);
	if (!Member.WorldBounds.IsValid)
	{
		return false;
	}

	const int32 MemberIndex = Members.Add(Member);
	KeyToIndex.Add(Key, MemberIndex);
	IndexMemberCells(MemberIndex, Member.WorldBounds);
	return true;
}

void FStructuralLightweightIndex::UnregisterMember(const FStructuralLightweightKey& Key)
{
	if (UFGStructuralPowerConnectionComponent* Connector = HiddenConnectors.FindRef(Key))
	{
		if (IsValid(Connector))
		{
			TArray<UFGCircuitConnectionComponent*> HiddenLinks;
			Connector->GetHiddenConnections(HiddenLinks);
			for (UFGCircuitConnectionComponent* Other : HiddenLinks)
			{
				if (UFGPowerConnectionComponent* OtherPower = Cast<UFGPowerConnectionComponent>(Other))
				{
					Connector->RemoveHiddenConnection(OtherPower);
				}
			}
		}
		HiddenConnectors.Remove(Key);
	}

	const int32* MemberIndexPtr = KeyToIndex.Find(Key);
	if (!MemberIndexPtr || !Members.IsValidIndex(*MemberIndexPtr))
	{
		KeyToIndex.Remove(Key);
		return;
	}

	const int32 RemoveIndex = *MemberIndexPtr;
	const int32 LastIndex = Members.Num() - 1;
	if (RemoveIndex != LastIndex)
	{
		Members.Swap(RemoveIndex, LastIndex);
	}

	Members.SetNum(LastIndex);
	KeyToIndex.Remove(Key);

	CellToIndices.Reset();
	KeyToIndex.Reset();
	for (int32 MemberIndex = 0; MemberIndex < Members.Num(); ++MemberIndex)
	{
		KeyToIndex.Add(Members[MemberIndex].Key, MemberIndex);
		IndexMemberCells(MemberIndex, Members[MemberIndex].WorldBounds);
	}
}

FStructuralWallAnchor FStructuralLightweightIndex::FindParentWallForOutlet(AFGBuildable* Outlet) const
{
	if (!IsValid(Outlet) || Members.Num() == 0)
	{
		return {};
	}

	const FVector AnchorLocation = FStructuralOutletParentHeuristics::GetOutletAnchorLocation(Outlet);
	const FIntVector Origin = ToCell(AnchorLocation);
	const int32 CellRadius = FMath::CeilToInt(
		StructuralPowerConstants::MaxOutletParentDistCm / StructuralPowerConstants::GridCellCm);

	const bool bPreferFoundation = FStructuralOutletParentHeuristics::PrefersFoundationAnchor(Outlet);

	FStructuralWallAnchor BestPreferred;
	float BestPreferredScore = TNumericLimits<float>::Max();
	FStructuralWallAnchor BestAny;
	float BestAnyScore = TNumericLimits<float>::Max();

	for (int32 DX = -CellRadius; DX <= CellRadius; ++DX)
	{
		for (int32 DY = -CellRadius; DY <= CellRadius; ++DY)
		{
			for (int32 DZ = -CellRadius; DZ <= CellRadius; ++DZ)
			{
				const TArray<int32>* Indices = CellToIndices.Find(Origin + FIntVector(DX, DY, DZ));
				if (!Indices)
				{
					continue;
				}

				for (int32 MemberIndex : *Indices)
				{
					const FIndexedMember& Member = Members[MemberIndex];
					const AFGBuildable* CDO = Member.Key.BuildableClass->GetDefaultObject<AFGBuildable>();

					const bool bNear = FStructuralOutletParentHeuristics::IsOutletNearBounds(
						Member.WorldBounds,
						Member.Key.BuildableClass,
						Outlet);
					if (!bNear)
					{
						continue;
					}

					FStructuralWallAnchor Candidate;
					Candidate.Lightweight = Member.Key;
					Candidate.WorldLocation = Member.WorldBounds.GetClosestPointTo(AnchorLocation);
					const float Score = FStructuralOutletParentHeuristics::ScoreParentCandidate(
						AnchorLocation,
						Member.WorldBounds,
						Member.Key.BuildableClass,
						Outlet);
					const bool bPreferredClass = bPreferFoundation
						? FStructuralOutletParentHeuristics::IsPreferredFoundationClass(CDO)
						: FStructuralOutletParentHeuristics::IsPreferredWallClass(CDO);
					if (bPreferredClass && Score < BestPreferredScore)
					{
						BestPreferredScore = Score;
						BestPreferred = Candidate;
					}
					else if (Score < BestAnyScore)
					{
						BestAnyScore = Score;
						BestAny = Candidate;
					}
				}
			}
		}
	}

	return BestPreferred.IsValid() ? BestPreferred : BestAny;
}

UFGStructuralPowerConnectionComponent* FStructuralLightweightIndex::FindHiddenConnector(
	const FStructuralLightweightKey& Key) const
{
	if (const UFGStructuralPowerConnectionComponent* const* Found = HiddenConnectors.Find(Key))
	{
		return const_cast<UFGStructuralPowerConnectionComponent*>(*Found);
	}

	return nullptr;
}

UFGStructuralPowerConnectionComponent* FStructuralLightweightIndex::GetOrCreateHiddenConnector(
	UWorld* World,
	const FStructuralLightweightKey& Key,
	const FVector& WorldLocation)
{
	if (UFGStructuralPowerConnectionComponent* Existing = FindHiddenConnector(Key))
	{
		if (IsValid(Existing))
		{
			Existing->SetWorldLocation(WorldLocation);
			return Existing;
		}
		HiddenConnectors.Remove(Key);
	}

	AFGLightweightBuildableSubsystem* Subsystem = GetLightweightSubsystem(World);
	if (!IsValid(Subsystem) || !Key.IsValid())
	{
		return nullptr;
	}

	const FName ConnectorName = *FString::Printf(
		TEXT("StructuralPowerLW_%s_%d"),
		*Key.BuildableClass->GetName(),
		Key.Index);

	TInlineComponentArray<UFGStructuralPowerConnectionComponent*> Connectors;
	Subsystem->GetComponents(Connectors);
	for (UFGStructuralPowerConnectionComponent* Connector : Connectors)
	{
		if (IsValid(Connector) && Connector->GetFName() == ConnectorName)
		{
			Connector->SetWorldLocation(WorldLocation);
			HiddenConnectors.Add(Key, Connector);
			return Connector;
		}
	}

	UFGStructuralPowerConnectionComponent* Connector =
		NewObject<UFGStructuralPowerConnectionComponent>(Subsystem, ConnectorName);
	if (!Connector)
	{
		return nullptr;
	}

	Connector->SetMobility(EComponentMobility::Static);
	Connector->SetIsHidden(true);
	Connector->SetWorldLocation(WorldLocation);
	Subsystem->AddInstanceComponent(Connector);
	Connector->RegisterComponent();
	HiddenConnectors.Add(Key, Connector);
	return Connector;
}

int32 FStructuralLightweightIndex::StitchPlacementNeighbors(
	UWorld* World,
	const FStructuralLightweightKey& Key,
	TFunctionRef<bool(
		UFGStructuralPowerConnectionComponent*,
		UFGStructuralPowerConnectionComponent*,
		const FStructuralLightweightKey&,
		const FStructuralLightweightKey&)> LinkFn)
{
	const FIndexedMember* Member = FindMember(Key);
	if (!Member)
	{
		return 0;
	}

	UFGStructuralPowerConnectionComponent* SelfHidden =
		GetOrCreateHiddenConnector(World, Key, Member->WorldBounds.GetCenter());
	if (!IsValid(SelfHidden))
	{
		return 0;
	}

	TArray<FStructuralLightweightKey> NeighborKeys;
	FindModManagedNeighborsInBounds(Member->WorldBounds, NeighborKeys);

	int32 LinksAdded = 0;
	for (const FStructuralLightweightKey& NeighborKey : NeighborKeys)
	{
		if (NeighborKey == Key)
		{
			continue;
		}

		const FIndexedMember* Neighbor = FindMember(NeighborKey);
		if (!Neighbor || !AreBoundsStructurallyConnected(
			Member->WorldBounds,
			Neighbor->WorldBounds,
			Member->Key.BuildableClass,
			NeighborKey.BuildableClass))
		{
			continue;
		}

		UFGStructuralPowerConnectionComponent* NeighborHidden =
			GetOrCreateHiddenConnector(World, NeighborKey, Neighbor->WorldBounds.GetCenter());
		if (!IsValid(NeighborHidden))
		{
			continue;
		}

		if (LinkFn(SelfHidden, NeighborHidden, Key, NeighborKey))
		{
			++LinksAdded;
		}
	}

	return LinksAdded;
}

void FStructuralLightweightIndex::FindModManagedNeighborsInBounds(
	const FBox& WorldBounds,
	TArray<FStructuralLightweightKey>& OutKeys) const
{
	OutKeys.Reset();
	if (!WorldBounds.IsValid)
	{
		return;
	}

	const FBox QueryBounds = WorldBounds.ExpandBy(StructuralPowerConstants::OverlapPaddingCm);
	const FIntVector MinCell = ToCell(QueryBounds.Min) - FIntVector(1, 1, 1);
	const FIntVector MaxCell = ToCell(QueryBounds.Max) + FIntVector(1, 1, 1);
	TSet<int32> SeenIndices;

	for (int32 X = MinCell.X; X <= MaxCell.X; ++X)
	{
		for (int32 Y = MinCell.Y; Y <= MaxCell.Y; ++Y)
		{
			for (int32 Z = MinCell.Z; Z <= MaxCell.Z; ++Z)
			{
				const TArray<int32>* Indices = CellToIndices.Find(FIntVector(X, Y, Z));
				if (!Indices)
				{
					continue;
				}

				for (int32 MemberIndex : *Indices)
				{
					if (SeenIndices.Contains(MemberIndex))
					{
						continue;
					}

					SeenIndices.Add(MemberIndex);
					OutKeys.Add(Members[MemberIndex].Key);
				}
			}
		}
	}
}

void FStructuralLightweightIndex::RehydrateConnectorsFromSubsystem(UWorld* World)
{
	AFGLightweightBuildableSubsystem* Subsystem = GetLightweightSubsystem(World);
	if (!IsValid(Subsystem))
	{
		return;
	}

	int32 Rehydrated = 0;
	for (const FIndexedMember& Member : Members)
	{
		const FName ConnectorName = *FString::Printf(
			TEXT("StructuralPowerLW_%s_%d"),
			*Member.Key.BuildableClass->GetName(),
			Member.Key.Index);

		TInlineComponentArray<UFGStructuralPowerConnectionComponent*> Connectors;
		Subsystem->GetComponents(Connectors);
		for (UFGStructuralPowerConnectionComponent* Connector : Connectors)
		{
			if (IsValid(Connector) && Connector->GetFName() == ConnectorName)
			{
				HiddenConnectors.Add(Member.Key, Connector);
				++Rehydrated;
				break;
			}
		}
	}

	if (Rehydrated > 0)
	{
		UE_LOG(LogStructuralPower, Log,
			TEXT("Rehydrated %d/%d tracked lightweight hidden connectors"),
			Rehydrated,
			Members.Num());
	}
}

void FStructuralLightweightIndex::RegisterSavedMembers(UWorld* World, const TArray<FStructuralLightweightKey>& Keys)
{
	for (const FStructuralLightweightKey& Key : Keys)
	{
		RegisterTrackedMember(World, Key);
	}
}

FStructuralNodeId FStructuralLightweightIndex::MakeNodeId(const FStructuralLightweightKey& Key)
{
	FStructuralNodeId Id;
	Id.BuildableClass = FSoftClassPath(Key.BuildableClass);
	Id.LightweightIndex = Key.Index;
	Id.ActorName = NAME_None;
	return Id;
}

FStructuralLightweightKey FStructuralLightweightIndex::KeyFromNodeId(const FStructuralNodeId& NodeId)
{
	FStructuralLightweightKey Key;
	Key.BuildableClass = NodeId.BuildableClass.TryLoadClass<AFGBuildable>();
	Key.Index = NodeId.LightweightIndex;
	return Key;
}
