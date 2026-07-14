// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Graph/FStructuralOutletParentResolver.h"

#include "Buildables/FGBuildable.h"
#include "FGBuildableSubsystem.h"
#include "FGLightweightBuildableSubsystem.h"
#include "Graph/FStructuralAdjacencyHeuristics.h"
#include "Graph/FStructuralBusMemberSpatialIndex.h"
#include "Graph/FStructuralConnectivityGraph.h"
#include "Graph/FStructuralOutletParentHeuristics.h"
#include "Graph/FStructuralHostAttachAdapter.h"
#include "Lightweight/FStructuralLightweightIndex.h"
#include "Rules/FStructuralEligibilityRules.h"
#include "StructuralPowerConstants.h"
#include "StructuralPowerLog.h"

namespace
{
static void ConsiderCandidate(
	const FBox& Bounds,
	TSubclassOf<AFGBuildable> BuildableClass,
	const AFGBuildable* Outlet,
	FStructuralWallAnchor Candidate,
	FStructuralWallAnchor& BestPreferred,
	float& BestPreferredScore,
	FStructuralWallAnchor& BestAny,
	float& BestAnyScore)
{
	if (!FStructuralOutletParentHeuristics::IsOutletNearBounds(Bounds, BuildableClass, Outlet))
	{
		return;
	}

	const FVector AnchorLocation = FStructuralOutletParentHeuristics::GetOutletAnchorLocation(Outlet);
	const float Score = FStructuralOutletParentHeuristics::ScoreParentCandidate(
		AnchorLocation,
		Bounds,
		BuildableClass,
		Outlet);

	const AFGBuildable* CDO = BuildableClass ? BuildableClass->GetDefaultObject<AFGBuildable>() : nullptr;
	const bool bPreferFoundation = FStructuralOutletParentHeuristics::PrefersFoundationAnchor(Outlet);
	const bool bPreferredClass = CDO
		&& (bPreferFoundation
			? FStructuralOutletParentHeuristics::IsPreferredFoundationClass(CDO)
			: FStructuralOutletParentHeuristics::IsPreferredWallClass(CDO));

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

static FStructuralWallAnchor TryResolveAttachedLightweightParent(AFGBuildable* Outlet, UWorld* World)
{
	if (!IsValid(Outlet) || !IsValid(World))
	{
		return {};
	}

	AFGLightweightBuildableSubsystem* LightweightSubsystem = AFGLightweightBuildableSubsystem::Get(World);
	if (!IsValid(LightweightSubsystem))
	{
		return {};
	}

	const FVector AnchorLocation = FStructuralOutletParentHeuristics::GetOutletAnchorLocation(Outlet);
	const bool bWallOutlet = FStructuralOutletParentHeuristics::IsWallOutlet(Outlet);

	FStructuralWallAnchor Best;
	float BestScore = TNumericLimits<float>::Max();

	for (const TPair<TSubclassOf<AFGBuildable>, TArray<FRuntimeBuildableInstanceData>>& ClassPair :
		LightweightSubsystem->GetAllLightweightBuildableInstances())
	{
		if (!ClassPair.Key)
		{
			continue;
		}

		const AFGBuildable* CDO = ClassPair.Key->GetDefaultObject<AFGBuildable>();
		if (!FStructuralEligibilityRules::IsBusMember(CDO))
		{
			continue;
		}

		if (bWallOutlet && !FStructuralOutletParentHeuristics::IsPreferredWallClass(CDO))
		{
			continue;
		}

		for (int32 Index = 0; Index < ClassPair.Value.Num(); ++Index)
		{
			const FRuntimeBuildableInstanceData& RuntimeData = ClassPair.Value[Index];
			if (!RuntimeData.IsValidOnLoad())
			{
				continue;
			}

			FBox Bounds = RuntimeData.BoundingBox.IsValid
				? RuntimeData.BoundingBox.TransformBy(RuntimeData.Transform)
				: FBox(RuntimeData.Transform.GetLocation(), RuntimeData.Transform.GetLocation());

			if (!Bounds.IsValid)
			{
				const float Extent = StructuralPowerConstants::DefaultFoundationExtentCm;
				const FVector HalfExtents(Extent, Extent, Extent);
				Bounds = FBox(RuntimeData.Transform.GetLocation() - HalfExtents, RuntimeData.Transform.GetLocation() + HalfExtents);
			}

			if (!FStructuralOutletParentHeuristics::IsOutletNearBounds(Bounds, ClassPair.Key, Outlet))
			{
				continue;
			}

			const float Score = FStructuralOutletParentHeuristics::ScoreParentCandidate(
				AnchorLocation,
				Bounds,
				ClassPair.Key,
				Outlet);
			if (Score >= BestScore)
			{
				continue;
			}

			BestScore = Score;
			Best.Lightweight = {ClassPair.Key, Index};
			Best.WorldLocation = Bounds.GetClosestPointTo(AnchorLocation);
		}
	}

	if (Best.IsValid())
	{
		const float DistCm = FMath::Sqrt(FVector::DistSquared(Best.WorldLocation, AnchorLocation));
		UE_LOG(LogStructuralPower, Verbose,
			TEXT("[HALSP] outlet %s parent resolved via lw_face_attach lw=%s[%d] distCm=%.1f"),
			*Outlet->GetName(),
			*Best.Lightweight.BuildableClass->GetName(),
			Best.Lightweight.Index,
			DistCm);
	}

	return Best;
}

static FStructuralWallAnchor FindParentFromLiveBusMembers(
	AFGBuildable* Outlet,
	const FStructuralBusMemberSpatialIndex* BusMemberIndex)
{
	if (!IsValid(Outlet))
	{
		return {};
	}

	if (BusMemberIndex)
	{
		return BusMemberIndex->FindParentForOutlet(Outlet);
	}

	if (AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(Outlet->GetWorld()))
	{
		const FVector AnchorLocation = FStructuralOutletParentHeuristics::GetOutletAnchorLocation(Outlet);
		FStructuralWallAnchor BestPreferred;
		float BestPreferredScore = TNumericLimits<float>::Max();
		FStructuralWallAnchor BestAny;
		float BestAnyScore = TNumericLimits<float>::Max();

		for (AFGBuildable* Candidate : BuildableSubsystem->GetAllBuildablesRef())
		{
			if (!IsValid(Candidate) || Candidate == Outlet)
			{
				continue;
			}

			if (!FStructuralEligibilityRules::IsBusMember(Candidate))
			{
				continue;
			}

			FVector Origin;
			FVector Extent;
			Candidate->GetActorBounds(false, Origin, Extent);
			const FBox Bounds(Origin - Extent, Origin + Extent);

			FStructuralWallAnchor Anchor;
			Anchor.Actor = Candidate;
			Anchor.WorldLocation = Bounds.IsValid ? Bounds.GetClosestPointTo(AnchorLocation) : Candidate->GetActorLocation();

			ConsiderCandidate(
				Bounds,
				Candidate->GetClass(),
				Outlet,
				Anchor,
				BestPreferred,
				BestPreferredScore,
				BestAny,
				BestAnyScore);
		}

		const FStructuralWallAnchor Result = BestPreferred.IsValid() ? BestPreferred : BestAny;
		if (Result.IsValid())
		{
			UE_LOG(LogStructuralPower, Log,
				TEXT("[HALSP] outlet %s parent resolved via live_scan actor=%s lw=%s[%d]"),
				*Outlet->GetName(),
				IsValid(Result.Actor) ? *Result.Actor->GetName() : TEXT("null"),
				Result.Lightweight.IsValid() ? *Result.Lightweight.BuildableClass->GetName() : TEXT("null"),
				Result.Lightweight.IsValid() ? Result.Lightweight.Index : INDEX_NONE);
		}

		return Result;
	}

	return {};
}

static FStructuralWallAnchor TryResolveFromStructureGraph(
	AFGBuildable* Outlet,
	const FStructuralConnectivityGraph* Graph,
	const FStructuralOutletParentResolveParams& Params)
{
	if (!IsValid(Outlet) || !Graph)
	{
		return {};
	}

	const FBox Bounds = FStructuralAdjacencyHeuristics::GetActorAdjacencyBounds(Outlet);
	FStructuralNodeId BestNodeId;
	if (Graph->FindRootForBounds(Bounds, Outlet->GetClass(), &BestNodeId) == INDEX_NONE)
	{
		return {};
	}

	FStructuralWallAnchor Anchor;
	const FVector AnchorLocation = FStructuralOutletParentHeuristics::GetOutletAnchorLocation(Outlet);

	if (BestNodeId.ActorName != NAME_None && Params.ResolveActorFromNodeId)
	{
		if (AFGBuildable* ParentActor = Params.ResolveActorFromNodeId(BestNodeId))
		{
			Anchor.Actor = ParentActor;
			Anchor.WorldLocation = ParentActor->GetActorLocation();
			return Anchor;
		}
	}

	if (BestNodeId.IsLightweight())
	{
		TSubclassOf<AFGBuildable> BuildableClass = BestNodeId.BuildableClass.TryLoadClass<AFGBuildable>();
		Anchor.Lightweight = {BuildableClass, BestNodeId.LightweightIndex};
		if (Anchor.Lightweight.IsValid())
		{
			AFGLightweightBuildableSubsystem* LightweightSubsystem =
				AFGLightweightBuildableSubsystem::Get(Outlet->GetWorld());
			if (IsValid(LightweightSubsystem))
			{
				const TArray<FRuntimeBuildableInstanceData>* Instances =
					LightweightSubsystem->GetAllLightweightBuildableInstances().Find(
						Anchor.Lightweight.BuildableClass);
				if (Instances && Instances->IsValidIndex(Anchor.Lightweight.Index))
				{
					const FRuntimeBuildableInstanceData& RuntimeData = (*Instances)[Anchor.Lightweight.Index];
					FBox NodeBounds = RuntimeData.BoundingBox.IsValid
						? RuntimeData.BoundingBox.TransformBy(RuntimeData.Transform)
						: FBox(RuntimeData.Transform.GetLocation(), RuntimeData.Transform.GetLocation());
					if (NodeBounds.IsValid)
					{
						Anchor.WorldLocation = NodeBounds.GetClosestPointTo(AnchorLocation);
					}
				}
			}
		}
	}

	return Anchor.IsValid() ? Anchor : FStructuralWallAnchor{};
}

static void LogResolvedParent(
	AFGBuildable* Outlet,
	const FStructuralOutletParentResolveResult& Result)
{
	if (!Result.Anchor.IsValid())
	{
		return;
	}

	UE_LOG(LogStructuralPower, Verbose,
		TEXT("[HALSP] outlet %s parent via %s actor=%s lw=%s[%d]"),
		*Outlet->GetName(),
		FStructuralOutletParentResolver::FormatParentMethod(Result.Method),
		IsValid(Result.Anchor.Actor) ? *Result.Anchor.Actor->GetName() : TEXT("null"),
		Result.Anchor.Lightweight.IsValid() ? *Result.Anchor.Lightweight.BuildableClass->GetName() : TEXT("null"),
		Result.Anchor.Lightweight.IsValid() ? Result.Anchor.Lightweight.Index : INDEX_NONE);
}
}

FStructuralOutletParentResolveResult FStructuralOutletParentResolver::ResolveDetailed(
	AFGBuildable* Outlet,
	UWorld* World,
	const FStructuralLightweightIndex& LightweightIndex)
{
	FStructuralOutletParentResolveParams Params;
	Params.LightweightIndex = &LightweightIndex;
	return ResolveDetailed(Outlet, World, Params);
}

FStructuralOutletParentResolveResult FStructuralOutletParentResolver::ResolveDetailed(
	AFGBuildable* Outlet,
	UWorld* World,
	const FStructuralOutletParentResolveParams& Params)
{
	FStructuralOutletParentResolveResult Result;

	if (!IsValid(Outlet))
	{
		return Result;
	}

	if (AFGBuildable* ParentActor = Cast<AFGBuildable>(Outlet->GetParentBuildableActor()))
	{
		if (FStructuralEligibilityRules::IsValidOutletParent(ParentActor))
		{
			Result.Anchor = FStructuralWallAnchor{ParentActor, {}, ParentActor->GetActorLocation()};
			Result.Method = EStructuralOutletParentMethod::ActorParent;
			LogResolvedParent(Outlet, Result);
			return Result;
		}
	}

	if (Params.LightweightIndex)
	{
		if (FStructuralWallAnchor IndexAnchor = Params.LightweightIndex->FindParentWallForOutlet(Outlet);
			IndexAnchor.IsValid())
		{
			Result.Anchor = IndexAnchor;
			Result.Method = EStructuralOutletParentMethod::LightweightIndex;
			LogResolvedParent(Outlet, Result);
			return Result;
		}
	}

	if (FStructuralWallAnchor GraphAnchor = TryResolveFromStructureGraph(Outlet, Params.StructureGraph, Params);
		GraphAnchor.IsValid())
	{
		Result.Anchor = GraphAnchor;
		Result.Method = EStructuralOutletParentMethod::StructureGraph;
		LogResolvedParent(Outlet, Result);
		return Result;
	}

	if (Params.bAllowLiveScan)
	{
		if (FStructuralWallAnchor LiveAnchor = FindParentFromLiveBusMembers(Outlet, Params.BusMemberIndex);
			LiveAnchor.IsValid())
		{
			Result.Anchor = LiveAnchor;
			Result.Method = EStructuralOutletParentMethod::LiveScan;
			return Result;
		}
	}

	const bool bIndexedSpatialExhausted = Params.LightweightIndex
		&& Params.BusMemberIndex
		&& Params.StructureGraph;
	if (!bIndexedSpatialExhausted)
	{
		if (FStructuralWallAnchor FaceAnchor = TryResolveAttachedLightweightParent(Outlet, World);
			FaceAnchor.IsValid())
		{
			Result.Anchor = FaceAnchor;
			Result.Method = EStructuralOutletParentMethod::LightweightFaceAttach;
			return Result;
		}
	}

	return Result;
}

FBox FStructuralOutletParentResolver::BoundsForStructuralAnchor(
	const FStructuralWallAnchor& Anchor,
	UWorld* World)
{
	if (IsValid(Anchor.Actor))
	{
		FVector Origin;
		FVector Extent;
		Anchor.Actor->GetActorBounds(false, Origin, Extent);
		return FBox(Origin - Extent, Origin + Extent);
	}

	if (Anchor.Lightweight.IsValid() && IsValid(World))
	{
		if (AFGLightweightBuildableSubsystem* LightweightSubsystem =
				AFGLightweightBuildableSubsystem::Get(World))
		{
			const TArray<FRuntimeBuildableInstanceData>* Instances =
				LightweightSubsystem->GetAllLightweightBuildableInstances().Find(
					Anchor.Lightweight.BuildableClass);
			if (Instances && Instances->IsValidIndex(Anchor.Lightweight.Index))
			{
				const FRuntimeBuildableInstanceData& RuntimeData =
					(*Instances)[Anchor.Lightweight.Index];
				if (RuntimeData.BoundingBox.IsValid)
				{
					return RuntimeData.BoundingBox.TransformBy(RuntimeData.Transform);
				}
			}
		}
	}

	if (!Anchor.WorldLocation.IsZero())
	{
		const float Extent = StructuralPowerConstants::DefaultFoundationExtentCm;
		const FVector HalfExtents(Extent, Extent, Extent);
		return FBox(Anchor.WorldLocation - HalfExtents, Anchor.WorldLocation + HalfExtents);
	}

	return FBox();
}

bool FStructuralOutletParentResolver::IsStructurallyAnchored(
	const FStructuralOutletParentResolveResult& Result,
	AFGBuildable* Outlet)
{
	return FStructuralHostAttachAdapter::ConfirmSiteAttach(Result, Outlet);
}

const TCHAR* FStructuralOutletParentResolver::FormatParentMethod(
	const EStructuralOutletParentMethod Method)
{
	switch (Method)
	{
	case EStructuralOutletParentMethod::ActorParent:
		return TEXT("actor_parent");
	case EStructuralOutletParentMethod::LightweightFaceAttach:
		return TEXT("lw_face_attach");
	case EStructuralOutletParentMethod::LightweightIndex:
		return TEXT("lw_index");
	case EStructuralOutletParentMethod::StructureGraph:
		return TEXT("structure_graph");
	case EStructuralOutletParentMethod::LiveScan:
		return TEXT("live_scan");
	default:
		return TEXT("none");
	}
}

FStructuralWallAnchor FStructuralOutletParentResolver::Resolve(
	AFGBuildable* Outlet,
	UWorld* World,
	const FStructuralLightweightIndex& LightweightIndex)
{
	return ResolveDetailed(Outlet, World, LightweightIndex).Anchor;
}

FStructuralWallAnchor FStructuralOutletParentResolver::Resolve(
	AFGBuildable* Outlet,
	UWorld* World,
	const FStructuralOutletParentResolveParams& Params)
{
	return ResolveDetailed(Outlet, World, Params).Anchor;
}
