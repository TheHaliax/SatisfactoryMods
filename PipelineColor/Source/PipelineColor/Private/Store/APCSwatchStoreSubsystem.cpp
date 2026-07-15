// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Store/APCSwatchStoreSubsystem.h"

#include "Appearance/FPCAppearanceSpec.h"
#include "Appearance/FPCFluidAppearanceCatalog.h"
#include "Core/FPCWorldGate.h"
#include "FGFactoryColoringTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "PipelineColorLog.h"
#include "Resources/FGItemDescriptor.h"
#include "Swatches/UPCSwatchDescs.h"
#include "UObject/SoftObjectPath.h"

APCSwatchStoreSubsystem::APCSwatchStoreSubsystem()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
}

void APCSwatchStoreSubsystem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APCSwatchStoreSubsystem, Entries);
}

void APCSwatchStoreSubsystem::PostLoadGame_Implementation(int32 /*SaveVersion*/, int32 /*GameVersion*/)
{
	RebuildMaps();
}

void APCSwatchStoreSubsystem::OnRep_Entries()
{
	RebuildMaps();
}

APCSwatchStoreSubsystem* APCSwatchStoreSubsystem::Find(UWorld* World)
{
	if (!IsValid(World))
	{
		return nullptr;
	}
	return Cast<APCSwatchStoreSubsystem>(
		UGameplayStatics::GetActorOfClass(World, StaticClass()));
}

APCSwatchStoreSubsystem* APCSwatchStoreSubsystem::GetOrCreate(UWorld* World)
{
	if (!FPCWorldGate::IsGameplayWorld(World) || World->GetNetMode() == NM_Client)
	{
		return Find(World);
	}

	if (APCSwatchStoreSubsystem* Existing = Find(World))
	{
		return Existing;
	}

	FActorSpawnParameters Params;
	Params.Name = TEXT("PipelineColorSwatchStore");
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<APCSwatchStoreSubsystem>(StaticClass(), FTransform::Identity, Params);
}

bool APCSwatchStoreSubsystem::IsPCCustomization(
	TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Swatch)
{
	return Swatch && Swatch->IsChildOf(UPCSwatchDescBase::StaticClass());
}

void APCSwatchStoreSubsystem::RebuildMaps()
{
	KeyToIndex.Reset();
	for (int32 i = 0; i < Entries.Num(); ++i)
	{
		if (!Entries[i].Key.IsNone())
		{
			KeyToIndex.Add(Entries[i].Key, i);
		}
	}
}

int32 APCSwatchStoreSubsystem::FindIndex(FName Key) const
{
	if (const int32* Idx = KeyToIndex.Find(Key))
	{
		return *Idx;
	}
	return INDEX_NONE;
}

FName APCSwatchStoreSubsystem::KeyFromSwatchStatic(
	TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Swatch)
{
	return UPCSwatchDescBase::GetCatalogKey(Swatch);
}

FName APCSwatchStoreSubsystem::KeyFromSwatch(
	TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Swatch) const
{
	return KeyFromSwatchStatic(Swatch);
}

bool APCSwatchStoreSubsystem::TryGet(FName Key, FPCSwatchEntry& Out) const
{
	const int32 Idx = FindIndex(Key);
	if (Idx == INDEX_NONE)
	{
		return false;
	}
	Out = Entries[Idx];
	return true;
}

bool APCSwatchStoreSubsystem::TryGetBySwatch(
	TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Swatch,
	FPCSwatchEntry& Out) const
{
	return TryGet(KeyFromSwatch(Swatch), Out);
}

void APCSwatchStoreSubsystem::Set(FName Key, const FPCSwatchEntry& Entry)
{
	if (Key.IsNone() || !HasAuthority())
	{
		return;
	}

	FPCSwatchEntry Copy = Entry;
	Copy.Key = Key;

	const int32 Idx = FindIndex(Key);
	if (Idx == INDEX_NONE)
	{
		KeyToIndex.Add(Key, Entries.Num());
		Entries.Add(Copy);
	}
	else
	{
		Entries[Idx] = Copy;
	}

	EntryChanged.Broadcast(Key);
	ForceNetUpdate();
}

void APCSwatchStoreSubsystem::SetFromSlot(FName Key, const FFactoryCustomizationColorSlot& Slot)
{
	FPCSwatchEntry Entry;
	Entry.FromSlot(Key, Slot);
	Set(Key, Entry);
}

void APCSwatchStoreSubsystem::SeedMissingFromCatalog()
{
	RebuildMaps();
	FPCFluidAppearanceCatalog& Catalog = FPCFluidAppearanceCatalog::Get();
	Catalog.EnsureLoaded();

	auto EnsureKey = [this](FName Key, const FPCAppearanceSpec& Spec)
	{
		if (FindIndex(Key) != INDEX_NONE)
		{
			return;
		}
		FPCSwatchEntry Entry;
		Entry.Key = Key;
		Entry.Primary = Spec.PrimaryColor;
		Entry.Secondary = Spec.SecondaryColor;
		Entry.PaintFinish = Spec.PaintFinish
			? FSoftClassPath(Spec.PaintFinish.Get())
			: FSoftClassPath();
		KeyToIndex.Add(Key, Entries.Num());
		Entries.Add(Entry);
	};

	{
		FPCAppearanceSpec Neutral;
		Catalog.Resolve(nullptr, true, Neutral);
		EnsureKey(FName(TEXT("Neutral")), Neutral);
	}

	struct FFluidSeed
	{
		FName Key;
		const TCHAR* Path;
	};
	const FFluidSeed Fluids[] = {
		{FName(TEXT("Water")),
			TEXT("/Game/FactoryGame/Resource/RawResources/Water/Desc_Water.Desc_Water_C")},
		{FName(TEXT("CrudeOil")),
			TEXT("/Game/FactoryGame/Resource/RawResources/CrudeOil/Desc_LiquidOil.Desc_LiquidOil_C")},
		{FName(TEXT("HeavyOilResidue")),
			TEXT("/Game/FactoryGame/Resource/Parts/HeavyOilResidue/Desc_HeavyOilResidue.Desc_HeavyOilResidue_C")},
		{FName(TEXT("Fuel")),
			TEXT("/Game/FactoryGame/Resource/Parts/Fuel/Desc_LiquidFuel.Desc_LiquidFuel_C")},
		{FName(TEXT("Turbofuel")),
			TEXT("/Game/FactoryGame/Resource/Parts/Turbofuel/Desc_TurboFuel.Desc_TurboFuel_C")},
		{FName(TEXT("LiquidBiofuel")),
			TEXT("/Game/FactoryGame/Resource/Parts/BioFuel/Desc_LiquidBiofuel.Desc_LiquidBiofuel_C")},
		{FName(TEXT("AluminaSolution")),
			TEXT("/Game/FactoryGame/Resource/Parts/Alumina/Desc_AluminaSolution.Desc_AluminaSolution_C")},
		{FName(TEXT("SulfuricAcid")),
			TEXT("/Game/FactoryGame/Resource/Parts/SulfuricAcid/Desc_SulfuricAcid.Desc_SulfuricAcid_C")},
		{FName(TEXT("DissolvedSilica")),
			TEXT("/Game/FactoryGame/Resource/Parts/DissolvedSilica/Desc_DissolvedSilica.Desc_DissolvedSilica_C")},
		{FName(TEXT("NitricAcid")),
			TEXT("/Game/FactoryGame/Resource/Parts/NitricAcid/Desc_NitricAcid.Desc_NitricAcid_C")},
		{FName(TEXT("DarkMatterResidue")),
			TEXT("/Game/FactoryGame/Resource/Parts/DarkEnergy/Desc_DarkEnergy.Desc_DarkEnergy_C")},
		{FName(TEXT("ExcitedPhotonicMatter")),
			TEXT("/Game/FactoryGame/Resource/Parts/QuantumEnergy/Desc_QuantumEnergy.Desc_QuantumEnergy_C")},
		{FName(TEXT("IonizedFuel")),
			TEXT("/Game/FactoryGame/Resource/Parts/IonizedFuel/Desc_IonizedFuel.Desc_IonizedFuel_C")},
		{FName(TEXT("RocketFuel")),
			TEXT("/Game/FactoryGame/Resource/Parts/RocketFuel/Desc_RocketFuel.Desc_RocketFuel_C")},
		{FName(TEXT("NitrogenGas")),
			TEXT("/Game/FactoryGame/Resource/RawResources/NitrogenGas/Desc_NitrogenGas.Desc_NitrogenGas_C")},
	};

	for (const FFluidSeed& Seed : Fluids)
	{
		TSubclassOf<UFGItemDescriptor> Desc =
			FSoftClassPath(Seed.Path).TryLoadClass<UFGItemDescriptor>();
		FPCAppearanceSpec Spec;
		Catalog.Resolve(Desc, !Desc, Spec);
		EnsureKey(Seed.Key, Spec);
	}

	{
		FPCAppearanceSpec Fallback = Catalog.GetNeutral();
		Fallback.PrimaryColor = FLinearColor::FromSRGBColor(FColor::White);
		EnsureKey(FName(TEXT("Fallback")), Fallback);
	}

	RebuildMaps();
	UE_LOG(LogPipelineColor, Log, TEXT("%s store seeded (%d entries)"),
		PIPELINECOLOR_LOG_PREFIX, Entries.Num());
}

void APCSwatchStoreSubsystem::ForceReseedNeutralMatte()
{
	RebuildMaps();
	FPCFluidAppearanceCatalog& Catalog = FPCFluidAppearanceCatalog::Get();
	Catalog.EnsureLoaded();

	FPCAppearanceSpec Spec;
	Catalog.Resolve(nullptr, true, Spec);

	FPCSwatchEntry Entry;
	Entry.Key = FName(TEXT("Neutral"));
	Entry.Primary = Spec.PrimaryColor;
	Entry.Secondary = Spec.SecondaryColor;
	Entry.PaintFinish = Spec.PaintFinish
		? FSoftClassPath(Spec.PaintFinish.Get())
		: FSoftClassPath(TEXT(
			"/Game/FactoryGame/Buildable/-Shared/Customization/PaintFinishes/"
			"PaintFinishDesc_Matte.PaintFinishDesc_Matte_C"));

	const int32 Idx = FindIndex(Entry.Key);
	if (Idx == INDEX_NONE)
	{
		KeyToIndex.Add(Entry.Key, Entries.Num());
		Entries.Add(Entry);
	}
	else
	{
		Entries[Idx] = Entry;
	}
	EntryChanged.Broadcast(Entry.Key);
	RebuildMaps();
	UE_LOG(LogPipelineColor, Log, TEXT("%s Neutral reseeded Matte"),
		PIPELINECOLOR_LOG_PREFIX);
}
