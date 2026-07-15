// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Appearance/FPCFluidAppearanceCatalog.h"

#include "Appearance/FPCFluidRoster.h"
#include "PipelineColorLog.h"
#include "Swatches/UPCFinishDescs.h"
#include "Swatches/UPCSwatchDescs.h"
#include "UObject/SoftObjectPath.h"

FPCFluidAppearanceCatalog& FPCFluidAppearanceCatalog::Get()
{
	static FPCFluidAppearanceCatalog Instance;
	return Instance;
}

FLinearColor FPCFluidAppearanceCatalog::HexRgb(uint8 R, uint8 G, uint8 B)
{
	return FLinearColor::FromSRGBColor(FColor(R, G, B, 255));
}

TSubclassOf<UFGItemDescriptor> FPCFluidAppearanceCatalog::LoadFluidDesc(
	const TCHAR* SoftPath)
{
	const FSoftClassPath Path(SoftPath);
	UClass* Loaded = Path.TryLoadClass<UFGItemDescriptor>();
	if (!Loaded)
	{
		UE_LOG(LogPipelineColor, Warning,
			TEXT("%s catalog: failed load fluid %s"), PIPELINECOLOR_LOG_PREFIX, SoftPath);
	}
	return Loaded;
}

TSubclassOf<UFGFactoryCustomizationDescriptor_PaintFinish>
FPCFluidAppearanceCatalog::LoadFinish(const TCHAR* SoftPath, const TCHAR* Label)
{
	const FSoftClassPath Path(SoftPath);
	UClass* Loaded = Path.TryLoadClass<UFGFactoryCustomizationDescriptor_PaintFinish>();
	if (!Loaded)
	{
		UE_LOG(LogPipelineColor, Warning,
			TEXT("%s catalog: failed load %s"), PIPELINECOLOR_LOG_PREFIX, Label);
	}
	return Loaded;
}

void FPCFluidAppearanceCatalog::EnsureLoaded()
{
	if (bBuilt)
	{
		return;
	}
	BuildEntries();
	bBuilt = true;
}

void FPCFluidAppearanceCatalog::BuildEntries()
{
	FinishDefault = LoadFinish(
		TEXT("/Game/FactoryGame/Buildable/-Shared/Customization/PaintFinishes/"
			 "PaintFinishDesc_Default.PaintFinishDesc_Default_C"),
		TEXT("PaintFinishDesc_Default"));
	FinishMatte = LoadFinish(
		TEXT("/Game/FactoryGame/Buildable/-Shared/Customization/PaintFinishes/"
			 "PaintFinishDesc_Matte.PaintFinishDesc_Matte_C"),
		TEXT("PaintFinishDesc_Matte"));
	FinishMetallicColor = UPCFinish_MetallicColor::StaticClass();
	UPCFinish_MetallicColor::EnsureIconLoaded();

	NeutralSpec.SwatchDesc = UPCSwatchDesc_Neutral::StaticClass();
	NeutralSpec.PrimaryColor = HexRgb(0x6E, 0x6E, 0x6E);
	NeutralSpec.SecondaryColor = HexRgb(0x2A, 0x2A, 0x2A);
	NeutralSpec.PaintFinish = FinishMatte ? FinishMatte : FinishDefault;
	NeutralSpec.CatalogKey = FName(TEXT("Neutral"));
	NeutralSpec.bOverrideRoughness = false;

	ByDescriptor.Reset();
	for (const FPCFluidRosterRow& Row : FPCFluidRoster::FluidRows())
	{
		TSubclassOf<UFGItemDescriptor> Desc = LoadFluidDesc(Row.SoftPath);
		if (!Desc)
		{
			continue;
		}

		FPCFluidCatalogEntry Entry;
		Entry.FluidStem = Row.Stem;
		Entry.Primary = HexRgb(Row.PrimaryR, Row.PrimaryG, Row.PrimaryB);
		Entry.Finish = Row.Finish;
		Entry.SwatchClass = Row.SwatchClass;
		ByDescriptor.Add(Desc, Entry);
	}

	UE_LOG(LogPipelineColor, Log, TEXT("%s catalog ready (%d fluids)"),
		PIPELINECOLOR_LOG_PREFIX, ByDescriptor.Num());
}

TSubclassOf<UFGFactoryCustomizationDescriptor_PaintFinish>
FPCFluidAppearanceCatalog::GetFinishClass(EPCPaintFinishKind Kind) const
{
	const_cast<FPCFluidAppearanceCatalog*>(this)->EnsureLoaded();
	if (Kind == EPCPaintFinishKind::MetallicColor)
	{
		return FinishMetallicColor;
	}
	if (Kind == EPCPaintFinishKind::Matte)
	{
		return FinishMatte ? FinishMatte : FinishDefault;
	}
	return FinishDefault;
}

bool FPCFluidAppearanceCatalog::ResolveByKey(FName CatalogKey, FPCAppearanceSpec& OutSpec)
	const
{
	const_cast<FPCFluidAppearanceCatalog*>(this)->EnsureLoaded();
	if (CatalogKey.IsNone())
	{
		return false;
	}
	if (CatalogKey == FName(TEXT("Neutral")))
	{
		OutSpec = NeutralSpec;
		return true;
	}

	for (const TPair<TSubclassOf<UFGItemDescriptor>, FPCFluidCatalogEntry>& Pair :
		ByDescriptor)
	{
		if (Pair.Value.FluidStem == CatalogKey)
		{
			return Resolve(Pair.Key, false, OutSpec);
		}
	}

	if (CatalogKey == FName(TEXT("Fallback")))
	{
		OutSpec = NeutralSpec;
		OutSpec.SwatchDesc = UPCSwatchDesc_Fallback::StaticClass();
		OutSpec.CatalogKey = CatalogKey;
		OutSpec.PrimaryColor = FLinearColor::FromSRGBColor(FColor::White);
		OutSpec.PaintFinish = FinishDefault;
		return true;
	}
	return false;
}

bool FPCFluidAppearanceCatalog::Resolve(
	TSubclassOf<UFGItemDescriptor> FluidDescriptor,
	bool bEmpty,
	FPCAppearanceSpec& OutSpec) const
{
	const_cast<FPCFluidAppearanceCatalog*>(this)->EnsureLoaded();
	if (bEmpty || !FluidDescriptor)
	{
		OutSpec = NeutralSpec;
		return true;
	}

	if (const FPCFluidCatalogEntry* Found = ByDescriptor.Find(FluidDescriptor))
	{
		OutSpec.SwatchDesc = Found->SwatchClass;
		OutSpec.CatalogKey = Found->FluidStem;
		OutSpec.PrimaryColor = Found->Primary;
		OutSpec.SecondaryColor = HexRgb(0x2A, 0x2A, 0x2A);
		OutSpec.PaintFinish = GetFinishClass(Found->Finish);
		OutSpec.bOverrideRoughness = false;
		return true;
	}

	OutSpec.SwatchDesc = UPCSwatchDesc_Fallback::StaticClass();
	OutSpec.CatalogKey = FluidDescriptor->GetFName();
	OutSpec.PrimaryColor = UFGItemDescriptor::GetFluidColorLinear(FluidDescriptor);
	OutSpec.SecondaryColor = HexRgb(0x2A, 0x2A, 0x2A);
	OutSpec.PaintFinish = FinishDefault;
	return true;
}
