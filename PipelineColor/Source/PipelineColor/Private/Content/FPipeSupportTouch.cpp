// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Content/FPipeSupportTouch.h"

#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildablePipeline.h"
#include "Buildables/FGBuildablePipelineAttachment.h"
#include "Components/ActorComponent.h"
#include "EngineUtils.h"
#include "FGBuildableSubsystem.h"
#include "FGPipeConnectionComponent.h"
#include "UObject/SoftObjectPath.h"

namespace FPipeSupportTouch
{
namespace
{
constexpr float TouchRadiusUu = 50.f;

TSubclassOf<AFGBuildable> LoadFluidSupportParent(const TCHAR* SoftPath)
{
	const FSoftClassPath Path(SoftPath);
	return Path.TryLoadClass<AFGBuildable>();
}

const TArray<TSubclassOf<AFGBuildable>>& FluidSupportParents()
{
	static TArray<TSubclassOf<AFGBuildable>> Parents;
	static bool bReady = false;
	if (!bReady)
	{
		bReady = true;
		TSubclassOf<AFGBuildable> Loaded[] = {
			LoadFluidSupportParent(TEXT(
				"/Game/FactoryGame/Buildable/Factory/PipelineSupport/"
				"Build_PipelineSupport.Build_PipelineSupport_C")),
			LoadFluidSupportParent(TEXT(
				"/Game/FactoryGame/Buildable/Factory/PipelineSupport/"
				"Build_PipeSupportStackable.Build_PipeSupportStackable_C")),
			LoadFluidSupportParent(TEXT(
				"/Game/FactoryGame/Buildable/Factory/PipelineSupportWall/"
				"Build_PipelineSupportWall.Build_PipelineSupportWall_C")),
			LoadFluidSupportParent(TEXT(
				"/Game/FactoryGame/Buildable/Factory/PipelineSupportWallHole/"
				"Build_PipelineSupportWallHole.Build_PipelineSupportWallHole_C")),
		};
		for (TSubclassOf<AFGBuildable> Cls : Loaded)
		{
			if (Cls)
			{
				Parents.Add(Cls);
			}
		}
	}
	return Parents;
}

void AddUniqueSupport(TArray<AFGBuildable*>& Out, AFGBuildable* Candidate)
{
	if (!IsValid(Candidate))
	{
		return;
	}
	Out.AddUnique(Candidate);
}

UFGPipeConnectionComponentBase* FirstPipeSnapConn(AFGBuildable* Buildable)
{
	if (!IsValid(Buildable))
	{
		return nullptr;
	}

	TInlineComponentArray<UFGPipeConnectionComponentBase*> Conns(Buildable);
	for (UFGPipeConnectionComponentBase* Conn : Conns)
	{
		if (IsValid(Conn))
		{
			return Conn;
		}
	}
	return nullptr;
}

FVector SupportSnapWorldLoc(AFGBuildable* Support)
{
	if (UFGPipeConnectionComponentBase* Conn = FirstPipeSnapConn(Support))
	{
		return Conn->GetConnectorLocation(/*withClearance=*/false);
	}
	return Support->GetActorLocation();
}

bool IsMidSpanTouching(AFGBuildablePipeline* Pipe, AFGBuildable* Support)
{
	if (!IsValid(Pipe) || !IsValid(Support))
	{
		return false;
	}

	const FVector SnapLoc = SupportSnapWorldLoc(Support);
	const float Offset = Pipe->FindOffsetClosestToLocation(SnapLoc);
	FVector OnSpline = FVector::ZeroVector;
	FVector Dir = FVector::ZeroVector;
	Pipe->GetLocationAndDirectionAtOffset(Offset, OnSpline, Dir);
	return FVector::DistSquared(SnapLoc, OnSpline)
		<= (TouchRadiusUu * TouchRadiusUu);
}

AFGBuildablePipeline* PipeFromConn(UFGPipeConnectionComponentBase* Conn)
{
	if (!IsValid(Conn))
	{
		return nullptr;
	}
	UFGPipeConnectionComponentBase* Other = Conn->GetConnection();
	if (!IsValid(Other))
	{
		return nullptr;
	}
	return Cast<AFGBuildablePipeline>(Other->GetOwner());
}
} // namespace

bool IsPipeSupport(const AFGBuildable* Buildable)
{
	if (!IsValid(Buildable)
		|| Buildable->IsA<AFGBuildablePipeline>()
		|| Buildable->IsA<AFGBuildablePipelineAttachment>())
	{
		return false;
	}

	for (TSubclassOf<AFGBuildable> Parent : FluidSupportParents())
	{
		if (Parent && Buildable->IsA(Parent))
		{
			return true;
		}
	}
	return false;
}

AFGBuildablePipeline* FindTouchedPipe(AFGBuildable* Support)
{
	if (!IsPipeSupport(Support))
	{
		return nullptr;
	}

	TInlineComponentArray<UFGPipeConnectionComponentBase*> Conns(Support);
	for (UFGPipeConnectionComponentBase* Conn : Conns)
	{
		if (AFGBuildablePipeline* Pipe = PipeFromConn(Conn))
		{
			return Pipe;
		}
	}

	UWorld* World = Support->GetWorld();
	if (!IsValid(World))
	{
		return nullptr;
	}

	AFGBuildablePipeline* Best = nullptr;
	float BestDistSq = TouchRadiusUu * TouchRadiusUu;
	const FVector SnapLoc = SupportSnapWorldLoc(Support);

	auto Consider = [&](AFGBuildablePipeline* Pipe)
	{
		if (!IsValid(Pipe))
		{
			return;
		}
		const float Offset = Pipe->FindOffsetClosestToLocation(SnapLoc);
		FVector OnSpline = FVector::ZeroVector;
		FVector Dir = FVector::ZeroVector;
		Pipe->GetLocationAndDirectionAtOffset(Offset, OnSpline, Dir);
		const float DistSq = FVector::DistSquared(SnapLoc, OnSpline);
		if (DistSq <= BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Pipe;
		}
	};

	if (AFGBuildableSubsystem* Sub = AFGBuildableSubsystem::Get(World))
	{
		for (AFGBuildable* Buildable : Sub->GetAllBuildablesRef())
		{
			Consider(Cast<AFGBuildablePipeline>(Buildable));
		}
	}
	else
	{
		for (TActorIterator<AFGBuildablePipeline> It(World); It; ++It)
		{
			Consider(*It);
		}
	}

	return Best;
}

void CollectSupportsTouchingPipe(
	AFGBuildablePipeline* Pipe,
	TArray<AFGBuildable*>& OutSupports)
{
	OutSupports.Reset();
	if (!IsValid(Pipe))
	{
		return;
	}

	auto TryAddFromPipeConn = [&](UFGPipeConnectionComponentBase* Conn)
	{
		if (!IsValid(Conn))
		{
			return;
		}
		UFGPipeConnectionComponentBase* Other = Conn->GetConnection();
		if (!IsValid(Other))
		{
			return;
		}
		if (AFGBuildable* Owner = Cast<AFGBuildable>(Other->GetOwner()))
		{
			if (IsPipeSupport(Owner))
			{
				AddUniqueSupport(OutSupports, Owner);
			}
		}
	};

	TryAddFromPipeConn(Pipe->GetConnection0());
	TryAddFromPipeConn(Pipe->GetConnection1());

	UWorld* World = Pipe->GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	auto ConsiderSupport = [&](AFGBuildable* Candidate)
	{
		if (!IsPipeSupport(Candidate))
		{
			return;
		}

		TInlineComponentArray<UFGPipeConnectionComponentBase*> Conns(Candidate);
		for (UFGPipeConnectionComponentBase* Conn : Conns)
		{
			if (PipeFromConn(Conn) == Pipe)
			{
				AddUniqueSupport(OutSupports, Candidate);
				return;
			}
		}

		if (IsMidSpanTouching(Pipe, Candidate))
		{
			AddUniqueSupport(OutSupports, Candidate);
		}
	};

	if (AFGBuildableSubsystem* Sub = AFGBuildableSubsystem::Get(World))
	{
		for (AFGBuildable* Buildable : Sub->GetAllBuildablesRef())
		{
			ConsiderSupport(Buildable);
		}
	}
	else
	{
		for (TActorIterator<AFGBuildable> It(World); It; ++It)
		{
			ConsiderSupport(*It);
		}
	}
}
} // namespace FPipeSupportTouch
