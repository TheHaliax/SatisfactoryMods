// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "UI/FStructuralIdConfigTarget.h"

#include "Buildables/FGBuildable.h"
#include "Camera/PlayerCameraManager.h"
#include "FGPlayerController.h"
#include "Rules/FStructuralEligibilityRules.h"

bool FStructuralIdConfigTarget::PickFromView(
	AFGPlayerController* PlayerController,
	AFGBuildable*& OutBuildable)
{
	OutBuildable = nullptr;
	if (!IsValid(PlayerController))
	{
		return false;
	}

	APlayerCameraManager* Camera = PlayerController->PlayerCameraManager;
	if (!IsValid(Camera))
	{
		return false;
	}

	const FVector Start = Camera->GetCameraLocation();
	const FVector End = Start + Camera->GetCameraRotation().Vector() * TraceDistanceCm;

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(StructuralIdConfigTrace), /*bTraceComplex=*/true);
	Params.AddIgnoredActor(PlayerController->GetPawn());

	UWorld* World = PlayerController->GetWorld();
	if (!IsValid(World)
		|| !World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		return false;
	}

	AActor* HitActor = Hit.GetActor();
	while (IsValid(HitActor))
	{
		if (AFGBuildable* Buildable = Cast<AFGBuildable>(HitActor))
		{
			if (FStructuralEligibilityRules::IsIdConfigTarget(Buildable))
			{
				OutBuildable = Buildable;
				return true;
			}
		}

		HitActor = HitActor->GetAttachParentActor();
	}

	return false;
}

EStructuralChannel FStructuralIdConfigTarget::GetTargetChannel(const AFGBuildable* Buildable)
{
	return FStructuralEligibilityRules::ClassifyBuildable(Buildable);
}
