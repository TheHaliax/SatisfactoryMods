// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Appearance/FPCAppearanceSpec.h"
#include "Subsystems/WorldSubsystem.h"
#include "UPCWorldSubsystem.generated.h"

class AFGBuildable;

UCLASS()
class PIPELINECOLOR_API UPCWorldSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UPCWorldSubsystem* Get(UWorld* World);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return false; }
	virtual bool IsTickable() const override;

	void Enqueue(AFGBuildable* Buildable);
	void ScanWorld();
	void ProcessNow(AFGBuildable* Buildable);

	void OnWorldReady(UWorld* World);
	void BindSwatchStore(UWorld* World);

private:
	void DrainDirty(int32 MaxCount);
	void BudgetedEmptyPass(int32 MaxCount);
	void OnSwatchEntryChanged(FName Key);
	void OnMetallicConfigChanged();
	void ApplyStoreOverlay(FPCAppearanceSpec& Spec) const;
	void DirtyAllWatched();

	TSet<TWeakObjectPtr<AFGBuildable>> Dirty;
	TArray<TWeakObjectPtr<AFGBuildable>> WatchList;
	TSet<TWeakObjectPtr<AFGBuildable>> WatchMembership;
	TMap<TWeakObjectPtr<AFGBuildable>, FPCAppearanceSpec> LastApplied;

	FDelegateHandle StoreChangedHandle;
	FDelegateHandle ConfigChangedHandle;
	TWeakObjectPtr<class APCSwatchStoreSubsystem> BoundStore;

	float EmptyScanAccum = 0.f;
	int32 WatchCursor = 0;
	bool bAuthority = false;
	bool bWorldReadyDone = false;

	static constexpr float EmptyScanInterval = 0.5f;
	static constexpr int32 EmptyScanBatch = 32;
	static constexpr int32 DirtyBatch = 64;
};
