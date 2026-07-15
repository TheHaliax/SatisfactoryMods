// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Swatches/UPCSwatchDescs.h"

namespace
{
void InitSwatch(UPCSwatchDescBase* Self, const TCHAR* Name, FName Key)
{
	if (!Self)
	{
		return;
	}

	Self->mUseDisplayNameAndDescription = true;
	Self->mDisplayName = FText::FromString(Name);
	Self->mDescription = FText::FromString(TEXT("PipelineColor fluid swatch"));
	Self->ID = INDEX_CUSTOM_COLOR_SLOT;
	Self->CatalogKey = Key;
	Self->mValidBuildables.Reset();
}
} // namespace

UPCSwatchDescBase::UPCSwatchDescBase()
{
	mUseDisplayNameAndDescription = true;
	ID = INDEX_CUSTOM_COLOR_SLOT;
}

FName UPCSwatchDescBase::GetCatalogKey(
	TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Swatch)
{
	if (!Swatch)
	{
		return NAME_None;
	}
	if (const UPCSwatchDescBase* CDO =
		Cast<UPCSwatchDescBase>(Swatch->GetDefaultObject()))
	{
		return CDO->CatalogKey;
	}
	return NAME_None;
}

UPCSwatchDesc_Neutral::UPCSwatchDesc_Neutral()
{
	InitSwatch(this, TEXT("PC Neutral"), FName(TEXT("Neutral")));
}

UPCSwatchDesc_Water::UPCSwatchDesc_Water()
{
	InitSwatch(this, TEXT("PC Water"), FName(TEXT("Water")));
}

UPCSwatchDesc_CrudeOil::UPCSwatchDesc_CrudeOil()
{
	InitSwatch(this, TEXT("PC Crude Oil"), FName(TEXT("CrudeOil")));
}

UPCSwatchDesc_HeavyOilResidue::UPCSwatchDesc_HeavyOilResidue()
{
	InitSwatch(this, TEXT("PC Heavy Oil Residue"), FName(TEXT("HeavyOilResidue")));
}

UPCSwatchDesc_Fuel::UPCSwatchDesc_Fuel()
{
	InitSwatch(this, TEXT("PC Fuel"), FName(TEXT("Fuel")));
}

UPCSwatchDesc_Turbofuel::UPCSwatchDesc_Turbofuel()
{
	InitSwatch(this, TEXT("PC Turbofuel"), FName(TEXT("Turbofuel")));
}

UPCSwatchDesc_LiquidBiofuel::UPCSwatchDesc_LiquidBiofuel()
{
	InitSwatch(this, TEXT("PC Liquid Biofuel"), FName(TEXT("LiquidBiofuel")));
}

UPCSwatchDesc_AluminaSolution::UPCSwatchDesc_AluminaSolution()
{
	InitSwatch(this, TEXT("PC Alumina Solution"), FName(TEXT("AluminaSolution")));
}

UPCSwatchDesc_SulfuricAcid::UPCSwatchDesc_SulfuricAcid()
{
	InitSwatch(this, TEXT("PC Sulfuric Acid"), FName(TEXT("SulfuricAcid")));
}

UPCSwatchDesc_DissolvedSilica::UPCSwatchDesc_DissolvedSilica()
{
	InitSwatch(this, TEXT("PC Dissolved Silica"), FName(TEXT("DissolvedSilica")));
}

UPCSwatchDesc_NitricAcid::UPCSwatchDesc_NitricAcid()
{
	InitSwatch(this, TEXT("PC Nitric Acid"), FName(TEXT("NitricAcid")));
}

UPCSwatchDesc_DarkMatterResidue::UPCSwatchDesc_DarkMatterResidue()
{
	InitSwatch(this, TEXT("PC Dark Matter Residue"), FName(TEXT("DarkMatterResidue")));
}

UPCSwatchDesc_ExcitedPhotonicMatter::UPCSwatchDesc_ExcitedPhotonicMatter()
{
	InitSwatch(this, TEXT("PC Excited Photonic Matter"),
		FName(TEXT("ExcitedPhotonicMatter")));
}

UPCSwatchDesc_IonizedFuel::UPCSwatchDesc_IonizedFuel()
{
	InitSwatch(this, TEXT("PC Ionized Fuel"), FName(TEXT("IonizedFuel")));
}

UPCSwatchDesc_RocketFuel::UPCSwatchDesc_RocketFuel()
{
	InitSwatch(this, TEXT("PC Rocket Fuel"), FName(TEXT("RocketFuel")));
}

UPCSwatchDesc_NitrogenGas::UPCSwatchDesc_NitrogenGas()
{
	InitSwatch(this, TEXT("PC Nitrogen Gas"), FName(TEXT("NitrogenGas")));
}

UPCSwatchDesc_Fallback::UPCSwatchDesc_Fallback()
{
	InitSwatch(this, TEXT("PC Fallback"), FName(TEXT("Fallback")));
}
