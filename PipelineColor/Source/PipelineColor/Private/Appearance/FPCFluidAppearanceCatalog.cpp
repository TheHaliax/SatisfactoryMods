// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Appearance/FPCFluidAppearanceCatalog.h"

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

TSubclassOf<UFGItemDescriptor> FPCFluidAppearanceCatalog::LoadFluidDesc(const TCHAR* SoftPath)
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
FPCFluidAppearanceCatalog::LoadDefaultFinish()
{
	const FSoftClassPath Path(TEXT(
		"/Game/FactoryGame/Buildable/-Shared/Customization/PaintFinishes/"
		"PaintFinishDesc_Default.PaintFinishDesc_Default_C"));
	UClass* Loaded = Path.TryLoadClass<UFGFactoryCustomizationDescriptor_PaintFinish>();
	if (!Loaded)
	{
		UE_LOG(LogPipelineColor, Warning,
			TEXT("%s catalog: failed load PaintFinishDesc_Default"), PIPELINECOLOR_LOG_PREFIX);
	}
	return Loaded;
}

TSubclassOf<UFGFactoryCustomizationDescriptor_PaintFinish>
FPCFluidAppearanceCatalog::LoadMatteFinish()
{
	const FSoftClassPath Path(TEXT(
		"/Game/FactoryGame/Buildable/-Shared/Customization/PaintFinishes/"
		"PaintFinishDesc_Matte.PaintFinishDesc_Matte_C"));
	UClass* Loaded = Path.TryLoadClass<UFGFactoryCustomizationDescriptor_PaintFinish>();
	if (!Loaded)
	{
		UE_LOG(LogPipelineColor, Warning,
			TEXT("%s catalog: failed load PaintFinishDesc_Matte"), PIPELINECOLOR_LOG_PREFIX);
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
	FinishDefault = LoadDefaultFinish();
	FinishMatte = LoadMatteFinish();
	FinishMetallicColor = UPCFinish_MetallicColor::StaticClass();
	UPCFinish_MetallicColor::EnsureIconLoaded();

	NeutralSpec.SwatchDesc = UPCSwatchDesc_Neutral::StaticClass();
	NeutralSpec.PrimaryColor = HexRgb(0x6E, 0x6E, 0x6E);
	NeutralSpec.SecondaryColor = HexRgb(0x2A, 0x2A, 0x2A);
	NeutralSpec.PaintFinish = FinishMatte ? FinishMatte : FinishDefault;
	NeutralSpec.CatalogKey = FName(TEXT("Neutral"));
	NeutralSpec.bOverrideRoughness = false;

	struct FSeed
	{
		const TCHAR* SoftPath;
		FName Stem;
		FLinearColor Primary;
		EPCPaintFinishKind Finish;
		TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Swatch;
	};

	const FSeed Seeds[] = {
		{TEXT("/Game/FactoryGame/Resource/RawResources/Water/Desc_Water.Desc_Water_C"),
			FName(TEXT("Water")), HexRgb(0x7A, 0xB0, 0xD4), EPCPaintFinishKind::Default,
			UPCSwatchDesc_Water::StaticClass()},
		{TEXT("/Game/FactoryGame/Resource/RawResources/CrudeOil/Desc_LiquidOil.Desc_LiquidOil_C"),
			FName(TEXT("CrudeOil")), HexRgb(0x19, 0x00, 0x19), EPCPaintFinishKind::Default,
			UPCSwatchDesc_CrudeOil::StaticClass()},
		{TEXT("/Game/FactoryGame/Resource/Parts/HeavyOilResidue/Desc_HeavyOilResidue.Desc_HeavyOilResidue_C"),
			FName(TEXT("HeavyOilResidue")), HexRgb(0x6D, 0x2D, 0x78), EPCPaintFinishKind::Default,
			UPCSwatchDesc_HeavyOilResidue::StaticClass()},
		{TEXT("/Game/FactoryGame/Resource/Parts/Fuel/Desc_LiquidFuel.Desc_LiquidFuel_C"),
			FName(TEXT("Fuel")), HexRgb(0xEB, 0x7D, 0x15), EPCPaintFinishKind::Default,
			UPCSwatchDesc_Fuel::StaticClass()},
		{TEXT("/Game/FactoryGame/Resource/Parts/Turbofuel/Desc_TurboFuel.Desc_TurboFuel_C"),
			FName(TEXT("Turbofuel")), HexRgb(0xD4, 0x29, 0x2E), EPCPaintFinishKind::Default,
			UPCSwatchDesc_Turbofuel::StaticClass()},
		{TEXT("/Game/FactoryGame/Resource/Parts/BioFuel/Desc_LiquidBiofuel.Desc_LiquidBiofuel_C"),
			FName(TEXT("LiquidBiofuel")), HexRgb(0x3B, 0x53, 0x2C), EPCPaintFinishKind::Default,
			UPCSwatchDesc_LiquidBiofuel::StaticClass()},
		{TEXT("/Game/FactoryGame/Resource/Parts/Alumina/Desc_AluminaSolution.Desc_AluminaSolution_C"),
			FName(TEXT("AluminaSolution")), HexRgb(0xC1, 0xC1, 0xC1), EPCPaintFinishKind::Default,
			UPCSwatchDesc_AluminaSolution::StaticClass()},
		{TEXT("/Game/FactoryGame/Resource/Parts/SulfuricAcid/Desc_SulfuricAcid.Desc_SulfuricAcid_C"),
			FName(TEXT("SulfuricAcid")), HexRgb(0xFF, 0xFF, 0x00), EPCPaintFinishKind::Default,
			UPCSwatchDesc_SulfuricAcid::StaticClass()},
		{TEXT("/Game/FactoryGame/Resource/Parts/DissolvedSilica/Desc_DissolvedSilica.Desc_DissolvedSilica_C"),
			FName(TEXT("DissolvedSilica")), HexRgb(0xE2, 0xBE, 0xEE), EPCPaintFinishKind::Default,
			UPCSwatchDesc_DissolvedSilica::StaticClass()},
		{TEXT("/Game/FactoryGame/Resource/Parts/NitricAcid/Desc_NitricAcid.Desc_NitricAcid_C"),
			FName(TEXT("NitricAcid")), HexRgb(0xD9, 0xD9, 0xA2), EPCPaintFinishKind::Default,
			UPCSwatchDesc_NitricAcid::StaticClass()},
		{TEXT("/Game/FactoryGame/Resource/Parts/DarkEnergy/Desc_DarkEnergy.Desc_DarkEnergy_C"),
			FName(TEXT("DarkMatterResidue")), HexRgb(0xFD, 0xAF, 0xF9),
			EPCPaintFinishKind::MetallicColor, UPCSwatchDesc_DarkMatterResidue::StaticClass()},
		{TEXT("/Game/FactoryGame/Resource/Parts/QuantumEnergy/Desc_QuantumEnergy.Desc_QuantumEnergy_C"),
			FName(TEXT("ExcitedPhotonicMatter")), HexRgb(0x76, 0xF5, 0xE8),
			EPCPaintFinishKind::MetallicColor,
			UPCSwatchDesc_ExcitedPhotonicMatter::StaticClass()},
		{TEXT("/Game/FactoryGame/Resource/Parts/IonizedFuel/Desc_IonizedFuel.Desc_IonizedFuel_C"),
			FName(TEXT("IonizedFuel")), HexRgb(0xD5, 0x5F, 0x1A),
			EPCPaintFinishKind::MetallicColor, UPCSwatchDesc_IonizedFuel::StaticClass()},
		{TEXT("/Game/FactoryGame/Resource/Parts/RocketFuel/Desc_RocketFuel.Desc_RocketFuel_C"),
			FName(TEXT("RocketFuel")), HexRgb(0xBD, 0x25, 0x1A),
			EPCPaintFinishKind::MetallicColor, UPCSwatchDesc_RocketFuel::StaticClass()},
		{TEXT("/Game/FactoryGame/Resource/RawResources/NitrogenGas/Desc_NitrogenGas.Desc_NitrogenGas_C"),
			FName(TEXT("NitrogenGas")), HexRgb(0x59, 0x59, 0x59),
			EPCPaintFinishKind::MetallicColor, UPCSwatchDesc_NitrogenGas::StaticClass()},
	};

	ByDescriptor.Reset();
	for (const FSeed& Seed : Seeds)
	{
		TSubclassOf<UFGItemDescriptor> Desc = LoadFluidDesc(Seed.SoftPath);
		if (!Desc)
		{
			continue;
		}

		FPCFluidCatalogEntry Entry;
		Entry.FluidStem = Seed.Stem;
		Entry.Primary = Seed.Primary;
		Entry.Finish = Seed.Finish;
		Entry.SwatchClass = Seed.Swatch;
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

bool FPCFluidAppearanceCatalog::ResolveByKey(FName CatalogKey, FPCAppearanceSpec& OutSpec) const
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

	for (const TPair<TSubclassOf<UFGItemDescriptor>, FPCFluidCatalogEntry>& Pair : ByDescriptor)
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
		OutSpec.PaintFinish = FinishDefault;
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
